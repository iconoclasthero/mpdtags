package main

import (
  "flag"
  "bufio"
  "fmt"
  "log"
  "os"
  "strconv"
  "strings"

  "github.com/fhs/gompd/v2/mpd"
)

var (
  defaultLog = "/var/log/mpd/mpd.log"
  defaultLibPrefix = "/library/music"
  debug = true
)


func shellQuote(s string) string {
  return "'" + strings.ReplaceAll(s, "'", "'\\''") + "'"
}

func printSong(song map[string]string) {
  for k, v := range song {
    if k == "Last-Modified" {
      k = "LastModified"
    }
    fmt.Printf("%s='%s'\n", k, v)
  }
}

func parseMPDHostEnv() (host, socket, pass string) {
    v := os.Getenv("MPD_HOST")
    if v == "" {
        return
    }

    if at := strings.IndexByte(v, '@'); at >= 0 {
        pass = v[:at]
        v = v[at+1:]
    }

    if strings.Contains(v, "/") {
        socket = v
    } else {
        host = v
    }

    return
}

func defaultMPDSocket() string {
    if v := os.Getenv("MPD_SOCKET"); v != "" {
        return v
    }

    if xdg := os.Getenv("XDG_RUNTIME_DIR"); xdg != "" {
        p := xdg + "/mpd/socket"
        if _, err := os.Stat(p); err == nil {
            return p
        }
    }

    if _, err := os.Stat("/run/mpd/socket"); err == nil {
        return "/run/mpd/socket"
    }

    if home := os.Getenv("HOME"); home != "" {
        p := home + "/.mpd/socket"
        if _, err := os.Stat(p); err == nil {
            return p
        }
    }

    return ""
}

func dialMPD(cliHost string, cliPort int, cliSocket string) (*mpd.Client, error) {
    envHost, envSocket, envPass := parseMPDHostEnv()
    pass := envPass

    host := envHost
    socket := envSocket
    port := cliPort

    if cliHost != "" {
        host = cliHost
    }
    if cliSocket != "" {
        socket = cliSocket
    }

    if port == 0 {
        if v := os.Getenv("MPD_PORT"); v != "" {
            if p, _ := strconv.Atoi(v); p > 0 {
                port = p
            }
        }
        if port == 0 {
            port = 6600
        }
    }

    if socket == "" {
        socket = defaultMPDSocket()
    }

    var c *mpd.Client
    var err error

    if socket != "" {
        c, err = mpd.Dial("unix", socket)
    } else {
        if host == "" {
            host = "localhost"
        }
        c, err = mpd.Dial("tcp", fmt.Sprintf("%s:%d", host, port))
    }

    if err != nil {
        return nil, err
    }

    if pass != "" {
        if err := c.Command("password " + pass).OK(); err != nil {
            c.Close()
            return nil, fmt.Errorf("MPD auth failed: %v", err)
        }
    }

    return c, nil
}

func findLastPlayed(logpath string) (completed, player, file string, err error) {
  f, err := os.Open(logpath)
  if err != nil {
    return "", "", "", err
  }
  defer f.Close()

  scanner := bufio.NewScanner(f)
  var lastLine string

  for scanner.Scan() {
    line := scanner.Text()
    if strings.Contains(line, "player: ") {
      lastLine = line
    }
  }

  if lastLine == "" {
    return "", "", "", fmt.Errorf("no player entries found in log")
  }

  parts := strings.SplitN(lastLine, " ", 2)
  if len(parts) != 2 {
    return "", "", "", fmt.Errorf("malformed log line: %s", lastLine)
  }
  completed = parts[0]

  idx := strings.Index(parts[1], "player: ")
  if idx < 0 {
    return "", "", "", fmt.Errorf("no player state found: %s", lastLine)
  }
  rest := parts[1][idx+len("player: "):]

  sp := strings.Index(rest, " ")
  if sp < 0 {
    return "", "", "", fmt.Errorf("malformed player line: %s", lastLine)
  }
  player = rest[:sp]

  fileStart := strings.Index(rest, "\"")
  fileEnd := strings.LastIndex(rest, "\"")
  if fileStart < 0 || fileEnd <= fileStart {
    return "", "", "", fmt.Errorf("malformed file path: %s", lastLine)
  }
  file = rest[fileStart+1 : fileEnd]

  return completed, player, file, nil
}

func main() {
    var conn *mpd.Client
    var err error

    // Default values
    lastPath := defaultLog
    lastSeen := false

    // Manual pre-parse for --last
    args := os.Args[1:]
    newArgs := []string{}
    for _, a := range args {
        if strings.HasPrefix(a, "--last") {
            lastSeen = true
            // --last=/some/path
            if eq := strings.Index(a, "="); eq >= 0 {
                lastPath = a[eq+1:]
            } else {
                // --last alone, use default
                lastPath = defaultLog
            }
        } else {
            newArgs = append(newArgs, a)
        }
    }

    // Override os.Args for standard flag parsing
    os.Args = append([]string{os.Args[0]}, newArgs...)

    // Standard flags
    host := flag.String("host", "localhost", "MPD host")
    port := flag.Int("port", 6600, "MPD port")
    socket := flag.String("socket", "", "MPD socket path")
    current := flag.Bool("current", false, "current song")
    next := flag.Bool("next", false, "next song")
    status := flag.Bool("status", false, "MPD status")

    flag.Parse()

//    if *socket != "" {
//        conn, err = mpd.Dial("unix", *socket)
//    } else {
//        conn, err = mpd.Dial("tcp", fmt.Sprintf("%s:%d", *host, *port))
//    }

    conn, err = dialMPD(*host, *port, *socket)
    if err != nil {
        log.Fatalf("MPD connect error: %v", err)
    }
    defer conn.Close()

    if err != nil {
        log.Fatalf("MPD connect error: %v", err)
    }
    defer conn.Close()


// Path mode: positional argument
args = flag.Args()
if len(args) > 0 {
    path := args[0]

    var songs []mpd.Attrs
    var err error

    // Absolute path
    if strings.HasPrefix(path, "/") {
        if *socket == "" {
            fmt.Fprintf(os.Stderr, "mpdtags: absolute paths require MPD socket\n")
            os.Exit(1)
        }
        songs, err = conn.ListAllInfo(path)
    } else {
        // Relative path
        songs, err = conn.ListAllInfo(path)
    }

    if err != nil || len(songs) == 0 {
        fmt.Fprintf(os.Stderr, "mpdtags: file not found in MPD database\n")
        os.Exit(1)
    }

    printSong(songs[0])
    return
}

//    // Handle --last
//    if lastSeen {
//        completed, player, path, err := findLastPlayed(lastPath)
//        if err != nil {
//            fmt.Fprintf(os.Stderr, "mpdtags: %v\n", err)
//            os.Exit(1)
//        }
//
//        fmt.Printf("completed=%s\nplayer=%s\n", completed, player)
//
////        songs, err := conn.ListAllInfo(path)
////        if err != nil || len(songs) == 0 {
////            fmt.Fprintf(os.Stderr, "mpdtags: file not found in MPD database\n")
////            os.Exit(1)
////        }
////
////        printSong(songs[0])
////        return
//
//        songs, err := conn.ListAllInfo(path)
//
//        if err != nil || len(songs) == 0 {
//            // Absolute-path fallback ONLY if using unix socket
//            if *socket != "" && !strings.HasPrefix(path, "/") {
//                abs := defaultLibPrefix + "/" + path
//                songs, err = conn.ListAllInfo(abs)
//            }
//        }
//
//        if err != nil || len(songs) == 0 {
//            fmt.Fprintf(os.Stderr, "mpdtags: file not found in MPD database\n")
//            os.Exit(1)
//        }
//
//        printSong(songs[0])
//        return
//
//    }
    // handle --last
//    if lastSeen {
//        completed, player, path, err := findLastPlayed(lastPath)
//        if err != nil {
//            fmt.Fprintf(os.Stderr, "mpdtags: %v\n", err)
//            os.Exit(1)
//        }
//
//        fmt.Printf("completed=%s\nplayer=%s\n", completed, player)
//
//        // 1) try relative path
//        songs, err := conn.ListAllInfo(path)
//        if err == nil && len(songs) > 0 {
//            printSong(songs[0])
//            return
//        }
//
//        // 2) fallback: absolute path
//        absPath := defaultLibPrefix + "/" + path
//        songs, err = conn.ListAllInfo(absPath)
//        if err == nil && len(songs) > 0 {
//            printSong(songs[0])
//            return
//        }
//
//        // 3) hard failure
//        fmt.Fprintf(os.Stderr,
//            "mpdtags: unable to locate last song (relative or absolute): %s\n",
//            path,
//        )
//        os.Exit(1)
//    }

    // --- handle --last ---
    if lastSeen {
        completed, player, path, err := findLastPlayed(lastPath)
        if err != nil {
            fmt.Fprintf(os.Stderr, "mpdtags: %v\n", err)
            os.Exit(1)
        }

        fmt.Printf("completed=%s\nplayer=%s\n", completed, player)

        if debug {
            fmt.Fprintf(os.Stderr, "DEBUG: entered --last block, lastPath='%s', parsed path='%s'\n", lastPath, path)
        }

        // helper to try a path
        tryPath := func(p string) ([]map[string]string, error) {
            if debug {
                fmt.Fprintf(os.Stderr, "DEBUG: trying path='%s'\n", p)
            }

            mpdSongs, err := conn.ListAllInfo(p) // returns []mpd.Attrs
            songs := make([]map[string]string, len(mpdSongs))
            for i, s := range mpdSongs {
                songs[i] = map[string]string(s) // convert to []map[string]string
            }

            if debug {
                fmt.Fprintf(os.Stderr, "DEBUG: ListAllInfo('%s') returned err=%v, len(songs)=%d\n", p, err, len(songs))
                for i, a := range songs {
                    fmt.Printf("DEBUG MPD ATTRS[%d]: %#v\n", i, a)
                }
            }

            return songs, err
        }

        // 1) try relative path
        songs, err := tryPath(path)
        if err == nil && len(songs) > 0 && songs[0]["file"] != "" {
            printSong(songs[0])
            return
        }

        // 2) fallback: absolute path
        absPath := defaultLibPrefix + "/" + path
        songs, err = tryPath(absPath)
        if err == nil && len(songs) > 0 && songs[0]["file"] != "" {
            printSong(songs[0])
            return
        }

        // 3) hard failure
        fmt.Fprintf(os.Stderr,
            "mpdtags: unable to locate last song (relative or absolute): %s\n",
            path,
        )
        os.Exit(1)
    }

    // Standard flags
    if *current {
        song, err := conn.CurrentSong()
        if err != nil {
            log.Fatalf("current song error: %v", err)
        }
        if song == nil {
            fmt.Println("no current song")
        } else {
            printSong(song)
        }
    }

    if *next {
        st, err := conn.Status()
        if err != nil {
            fmt.Println("failed to get MPD status:", err)
        } else {
            nextIdx, _ := strconv.Atoi(st["nextsong"])
            songs, err := conn.PlaylistInfo(nextIdx, -1)
            if err != nil || len(songs) == 0 {
                fmt.Println("failed to get next song")
            } else {
                printSong(songs[0])
            }
        }
    }

    if *status {
        st, err := conn.Status()
        if err != nil {
            log.Fatalf("status error: %v", err)
        }
        for k, v := range st {
            fmt.Printf("%s: %s\n", k, v)
        }
    }
}

//func main() {
//  var lastPath string
//  var useLast bool
//
//  host := pflag.String("host", "localhost", "MPD host")
//  port := pflag.Int("port", 6600, "MPD port")
//  socket := pflag.String("socket", "", "MPD socket path")
//  current := pflag.Bool("current", false, "current song")
//  next := pflag.Bool("next", false, "next song")
//  status := pflag.Bool("status", false, "MPD status")
//
//  // optional value flag: --last or --last=/path/to/log
//  pflag.Func("last", "last played song (optional log path)", func(s string) error {
//    useLast = true
//    if s == "" {
//      lastPath = defaultLog
//    } else {
//      lastPath = s
//    }
//    return nil
//  })
//
//  pflag.Parse()
//
//  var conn *mpd.Client
//  var err error
//  if *socket != "" {
//    conn, err = mpd.Dial("unix", *socket)
//  } else {
//    conn, err = mpd.Dial("tcp", fmt.Sprintf("%s:%d", *host, *port))
//  }
//  if err != nil {
//    log.Fatalf("MPD connect error: %v", err)
//  }
//  defer conn.Close()
//
//  if useLast {
//    completed, player, path, err := findLastPlayed(lastPath)
//    if err != nil {
//      fmt.Fprintf(os.Stderr, "mpdtags: %v\n", err)
//      os.Exit(1)
//    }
//
//    fmt.Printf("completed=%s\nplayer=%s\n", completed, player)
//
//    songs, err := conn.ListAllInfo(path)
//    if err != nil || len(songs) == 0 {
//      fmt.Fprintf(os.Stderr, "mpdtags: file not found in MPD database\n")
//      os.Exit(1)
//    }
//
//    printSong(songs[0])
//    return
//  }
//
//  if *current {
//    song, err := conn.CurrentSong()
//    if err != nil {
//      log.Fatalf("current song error: %v", err)
//    }
//    if song == nil {
//      fmt.Println("no current song")
//    } else {
//      printSong(song)
//    }
//  }
//
//  if *next {
//    st, err := conn.Status()
//    if err != nil {
//      fmt.Println("failed to get next song:", err)
//    } else {
//      nextIdx, _ := strconv.Atoi(st["nextsong"])
//      songs, err := conn.PlaylistInfo(nextIdx, -1)
//      if err != nil || len(songs) == 0 {
//        fmt.Println("failed to get next song")
//      } else {
//        printSong(songs[0])
//      }
//    }
//  }
//
//  if *status {
//    st, err := conn.Status()
//    if err != nil {
//      log.Fatalf("status error: %v", err)
//    }
//    for k, v := range st {
//      fmt.Printf("%s: %s\n", k, v)
//    }
//  }
//}
