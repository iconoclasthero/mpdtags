package main

import (
	"bufio"
	"flag"
	"fmt"
	"log"
	"os"
	"strconv"
	"strings"

	"github.com/fhs/gompd/v2/mpd"
)

var defaultLog = "/var/log/mpd/mpd.log"

type lastFlag struct {
    path string
    seen bool
}

func (l *lastFlag) String() string { return l.path }
func (l *lastFlag) Set(value string) error {
    l.path = value
    l.seen = true
    return nil
}


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

var last string

func init() {
    last = defaultLog
    flag.StringVar(&last, "last", defaultLog, "last played song (optional log path)")
}

func main() {

	var conn *mpd.Client
	var err error
  var last lastFlag
  last.path = defaultLog

	host := flag.String("host", "localhost", "MPD host")
	port := flag.Int("port", 6600, "MPD port")
	socket := flag.String("socket", "", "MPD socket path")

	current := flag.Bool("current", false, "current song")
	next := flag.Bool("next", false, "next song")
	status := flag.Bool("status", false, "MPD status")

	flag.Parse()

	if *socket != "" {
		conn, err = mpd.Dial("unix", *socket)
	} else {
		conn, err = mpd.Dial("tcp", fmt.Sprintf("%s:%d", *host, *port))
	}
	if err != nil {
		log.Fatalf("MPD connect error: %v", err)
	}
	defer conn.Close()

  if last.seen {
      completed, player, path, err := findLastPlayed(last.path)
      if err != nil {
          fmt.Fprintf(os.Stderr, "mpdtags: %v\n", err)
          os.Exit(1)
      }

      fmt.Printf("completed=%s\nplayer=%s\n", completed, player)

      songs, err := conn.ListAllInfo(path)
      if err != nil || len(songs) == 0 {
          fmt.Fprintf(os.Stderr, "mpdtags: file not found in MPD database\n")
          os.Exit(1)
      }

      printSong(songs[0])
      return
  }

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

//	if *last {
//		completed, player, path, err := findLastPlayed(*lastlog)
//		if err != nil {
//			fmt.Fprintf(os.Stderr, "mpdtags: %v\n", err)
//			os.Exit(1)
//		}
//
//		fmt.Printf("completed=%s\n", completed)
//		fmt.Printf("player=%s\n", player)
//
//		songs, err := conn.ListAllInfo(path)
//		if err != nil || len(songs) == 0 {
//			fmt.Fprintf(os.Stderr, "mpdtags: file not found in MPD database\n")
//			os.Exit(1)
//		}
//
//		printSong(songs[0])
//	}

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
