package main

import (
	"flag"
	"fmt"
  "os"
  "bufio"
	"log"
	"strings"
  "strconv"
	"github.com/fhs/gompd/v2/mpd"
)

func shellQuote(s string) string {
	return "'" + strings.ReplaceAll(s, "'", "'\\''") + "'"
}

func printSong(song map[string]string) {
    for k, v := range song {
        fmt.Printf("%s='%s'\n", k, v)
    }
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

    // Split timestamp from rest
    parts := strings.SplitN(lastLine, " ", 2)
    if len(parts) != 2 {
        return "", "", "", fmt.Errorf("malformed log line: %s", lastLine)
    }
    completed = parts[0]

    // player: state "file"
    idx := strings.Index(parts[1], "player: ")
    if idx < 0 {
        return "", "", "", fmt.Errorf("no player state found: %s", lastLine)
    }
    rest := parts[1][idx+len("player: "):]

    // rest starts with state
    sp := strings.Index(rest, " ")
    if sp < 0 {
        return "", "", "", fmt.Errorf("malformed player line: %s", lastLine)
    }
    player = rest[:sp]

    // quoted file path
    fileStart := strings.Index(rest, "\"")
    fileEnd := strings.LastIndex(rest, "\"")
    if fileStart < 0 || fileEnd <= fileStart {
        return "", "", "", fmt.Errorf("malformed file path: %s", lastLine)
    }
    file = rest[fileStart+1 : fileEnd]

    return completed, player, file, nil
}

//func main() {
//	host := flag.String("host", "localhost", "MPD host")
//	port := flag.Int("port", 6600, "MPD port")
//	socket := flag.String("socket", "", "MPD socket path")
//	current := flag.Bool("current", false, "current song")
//	next := flag.Bool("next", false, "next song")
//	status := flag.Bool("status", false, "MPD status")
//	flag.Parse()
//  last := flag.Bool("last", false, "last played song")
//  logpath := flag.String("lastlog", "/var/log/mpd/mpd.log", "MPD log path for --last")
//
//	var conn *mpd.Client
//	var err error
//
//	if *socket != "" {
//		conn, err = mpd.Dial("unix", *socket)
//	} else {
//		conn, err = mpd.Dial("tcp", fmt.Sprintf("%s:%d", *host, *port))
//	}
//	if err != nil {
//		log.Fatalf("MPD connect error: %v", err)
//	}
//	defer conn.Close()
//
//	if *current {
//		song, err := conn.CurrentSong()
//		if err != nil {
//			log.Fatalf("current song error: %v", err)
//		}
//		if song == nil {
//			fmt.Println("no current song")
//		} else {
//			printSong(song)
//		}
//	}
//
//  if *next {
//      status, err := conn.Status()
//      if err != nil {
//          fmt.Println("failed to get MPD status:", err)
//      } else {
//          nextIdx, _ := strconv.Atoi(status["nextsong"])
//          songs, err := conn.PlaylistInfo(nextIdx, -1)
//          if err != nil || len(songs) == 0 {
//              fmt.Println("failed to get next song")
//          } else {
//              printSong(songs[0])
//          }
//      }
//  }
//
//  if *last {
//      logpath := o.logpath
//      if logpath == "" {
//          logpath = "/var/log/mpd/mpd.log"
//      }
//      completed, player, path, err := findLastPlayed(logpath)
//      if err != nil {
//          fmt.Fprintf(os.Stderr, "mpdtags: %v\n", err)
//          os.Exit(1)
//      }
//
//      fmt.Printf("completed=%s\nplayer=%s\n", completed, player)
//      o.path = path
//      o.local = true
//  }
//
//	if *status {
//		st, err := conn.Status()
//		if err != nil {
//			log.Fatalf("status error: %v", err)
//		}
//		for k, v := range st {
//			fmt.Printf("%s: %s\n", k, v)
//		}
//	}
//}

func main() {
	host := flag.String("host", "localhost", "MPD host")
	port := flag.Int("port", 6600, "MPD port")
	socket := flag.String("socket", "", "MPD socket path")
	current := flag.Bool("current", false, "current song")
	next := flag.Bool("next", false, "next song")
	last := flag.Bool("last", false, "last played song")
	logpath := flag.String("lastlog", "/var/log/mpd/mpd.log", "MPD log path for --last")
	statusFlag := flag.Bool("status", false, "MPD status")
	flag.Parse()

	var conn *mpd.Client
	var err error

	if *socket != "" {
		conn, err = mpd.Dial("unix", *socket)
	} else {
		conn, err = mpd.Dial("tcp", fmt.Sprintf("%s:%d", *host, *port))
	}
	if err != nil {
		log.Fatalf("MPD connect error: %v", err)
	}
	defer conn.Close()

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
		status, err := conn.Status()
		if err != nil {
			fmt.Println("failed to get MPD status:", err)
		} else {
			nextIdx, _ := strconv.Atoi(status["nextsong"])
			songs, err := conn.PlaylistInfo(nextIdx, -1)
			if err != nil || len(songs) == 0 {
				fmt.Println("failed to get next song")
			} else {
				printSong(songs[0])
			}
		}
	}

	if *last {
		path := *logpath
		completed, player, file, err := findLastPlayed(path)
		if err != nil {
			fmt.Fprintf(os.Stderr, "mpdtags: %v\n", err)
			os.Exit(1)
		}

		fmt.Printf("completed=%s\nplayer=%s\nfile='%s'\n", completed, player, file)

		// optional: if you want metadata from MPD for the last file:
		// songs, err := conn.PlaylistInfo(file, -1)
		// if err == nil && len(songs) > 0 {
		//     printSong(songs[0])
		// }
	}

	if *statusFlag {
		st, err := conn.Status()
		if err != nil {
			log.Fatalf("status error: %v", err)
		}
		for k, v := range st {
			fmt.Printf("%s: %s\n", k, v)
		}
	}
}
