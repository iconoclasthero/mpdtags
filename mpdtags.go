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

var defaultLog = "/var/log/mpd/mpd.log"

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

func main() {
	var conn *mpd.Client
	var err error

	host := flag.String("host", "localhost", "MPD host")
	port := flag.Int("port", 6600, "MPD port")
	socket := flag.String("socket", "", "MPD socket path")

	current := flag.Bool("current", false, "current song")
	next := flag.Bool("next", false, "next song")
	status := flag.Bool("status", false, "MPD status")

	// parse all flags except --last
	flag.Parse()

	// Handle --last manually
	lastLog := defaultLog
	lastSeen := false
	for i, arg := range os.Args[1:] {
		if strings.HasPrefix(arg, "--last") {
			lastSeen = true
			if arg == "--last" {
				lastLog = defaultLog
			} else if strings.HasPrefix(arg, "--last=") {
				lastLog = strings.TrimPrefix(arg, "--last=")
			} else if arg == "--last" && i+2 <= len(os.Args[1:]) {
				lastLog = os.Args[i+2]
			}
			break
		}
	}

	// Connect to MPD
	if *socket != "" {
		conn, err = mpd.Dial("unix", *socket)
	} else {
		conn, err = mpd.Dial("tcp", fmt.Sprintf("%s:%d", *host, *port))
	}
	if err != nil {
		log.Fatalf("MPD connect error: %v", err)
	}
	defer conn.Close()

	// Handle --last
	if lastSeen {
		completed, player, path, err := findLastPlayed(lastLog)
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

	// Handle --current
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

	// Handle --next
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

	// Handle --status
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
//	var lastPath string
//	var useLast bool
//
//	host := pflag.String("host", "localhost", "MPD host")
//	port := pflag.Int("port", 6600, "MPD port")
//	socket := pflag.String("socket", "", "MPD socket path")
//	current := pflag.Bool("current", false, "current song")
//	next := pflag.Bool("next", false, "next song")
//	status := pflag.Bool("status", false, "MPD status")
//
//	// optional value flag: --last or --last=/path/to/log
//	pflag.Func("last", "last played song (optional log path)", func(s string) error {
//		useLast = true
//		if s == "" {
//			lastPath = defaultLog
//		} else {
//			lastPath = s
//		}
//		return nil
//	})
//
//	pflag.Parse()
//
//	var conn *mpd.Client
//	var err error
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
//	if useLast {
//		completed, player, path, err := findLastPlayed(lastPath)
//		if err != nil {
//			fmt.Fprintf(os.Stderr, "mpdtags: %v\n", err)
//			os.Exit(1)
//		}
//
//		fmt.Printf("completed=%s\nplayer=%s\n", completed, player)
//
//		songs, err := conn.ListAllInfo(path)
//		if err != nil || len(songs) == 0 {
//			fmt.Fprintf(os.Stderr, "mpdtags: file not found in MPD database\n")
//			os.Exit(1)
//		}
//
//		printSong(songs[0])
//		return
//	}
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
//	if *next {
//		st, err := conn.Status()
//		if err != nil {
//			fmt.Println("failed to get next song:", err)
//		} else {
//			nextIdx, _ := strconv.Atoi(st["nextsong"])
//			songs, err := conn.PlaylistInfo(nextIdx, -1)
//			if err != nil || len(songs) == 0 {
//				fmt.Println("failed to get next song")
//			} else {
//				printSong(songs[0])
//			}
//		}
//	}
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
