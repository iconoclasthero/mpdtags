# mpdtags

A small utility to fetch MPD song metadata, with optional socket fallback for local files.

## Usage

```
usage: mpdtags [options] <path>
  --host=HOST      : Specify the host URL/IP address for MPD
  --port=PORT      : Specify the host port for MPD
  --socket[=/path] : Force domain socket connection regardless of env variables**
  --local          : Specify <path> is a local file (for which TCP is prohibited)**
  --current        : Return info on the current song
  --next           : Return info on the next song
  --last[=/path]   : Return info on the last song in log; defaults to /var/log/mpd/mpd.log
                   : Use --last=/path/to/mpd.log or set MPD_LOG variable for alternate log path
  --status         : Return the MPD status information
  --help           : Display this message
  --version        : Display version and exit

**By default MPD will use the MPD_HOST and MPD_PORT environmental variables to connect via TCP
but disallows reading of local files over TCP. mpdtags will automatically try to use a domain
socket in such a case but --local/--socket will force this with the latter providing the optional
facility to specify a socket path.
```

## Rationale

Ever wanted to use the database—a gzipped text file—that MPD keeps of your music collection to **quickly** retrieve metadata tags for your music in your library? This allows you to use that database and thus obviates the need to keep a second, synchronized database of your music collection. One obvious caveat to note is that files intentionally excluded from the MPD database via `.mpdignore` files will not be present _in the database._

However, MPD has the ability to read from any local file (including ignored files) given an absolute path, but it is prohibited from doing so over TCP (e.g., from a remote connection), so a UNIX domain socket connection must be used. `libmpdclient` will preferentially attempt to use a TCP connection if the `MPD_HOST` and `MPD_PORT` environment variables are set, and will error out in this case.

If a UNIX domain socket exists, `mpdtags` will automatically attempt to connect via the socket if it receives an error from MPD (specifically: `Access to local files via TCP is not allowed`). If it is known in advance that MPD will be asked to retrieve metadata for an absolute path (regardless of whether it exists in the database), the `--local` or `--socket` options can be used. With --socket, a socket path may be specified explicitly, for example: `--socket=/var/run/mpd/socket`.
## Notes

- A static build exists in the release section that does not require `libmpdclient` to be installed.
- The --last song flag requires first reading the last song played from the mpd log and thus logging must be enabled via mpd's configuration file. `mpdtags` uses `/var/log/mpd/mpd.log` as the default location; alternate locations may be specified by using either `mpdtags --last=/path/to/mpd.log` or using the `MPD_LOG=/path/to/mpd.log` variable either on the CLI or as an environmental variable (e.g., `$ MPD_LOG=/parth/to/mpd.log mpdtags --last` OR `$ export MPD_LOG=/path/to/mpd.log; mpdtags --last`). NB: --last will only work if `mpdtags` has read access to the log (and will not work for remote connections).
- Speed is relative, but it takes about 2.5 seconds to retrieve the metadata from 1000 songs preprocessed from a log of random playback:
```
$ time while read -r line; do mpdtags "$line" > /dev/null; done < processed.mpd.log; wc -l processed.mpd.log

real  0m2.500s
user  0m1.209s
sys   0m0.900s
1000  processed.mpd.log
```


## Usage examples

### Retrieve information on the last song played from the log (uses relative path):
```
$ mpdtags "$(last="$(grep 'player: played' /var/log/mpd/mpd.log|tail -n1)"; last="${last%\"}"; last="${last#*\"}"; echo "$last")"
file='Bob Dylan/Various Artists/Various Artists -- Nobody Sings Dylan Like Dylan, Volume 30: Time Is the Enemy/01-13 - Don Henley -- Well Well Well [1993-09-06: The Walden Woods Benefit, Foxborough Stadium, Foxborough, MA].flac'
artist='Don Henley'
album='Nobody Sings Dylan Like Dylan, Volume 30: Time Is the Enemy'
albumartist='Various Artists'
title='Well Well Well [1993-09-06: The Walden Woods Benefit, Foxborough Stadium, Foxborough, MA]'
track='13'
genre='Folk, Pop Rock, Rock'
disc='1'
musicbrainz_artistid='b2c2d4fe-8c1e-44ec-8be6-ff500e105a90'
musicbrainz_albumid='779f282a-3c85-4f03-8e6c-5acf803660e9'
musicbrainz_albumartistid='89ad4ac3-39f7-470e-963a-56509c546377'
musicbrainz_trackid='1d6b580e-98a5-476c-95b6-0dd4e1a7fd2a'
musicbrainz_releasetrackid='0cba8888-1c52-4020-b164-a02d2e78ab96'
artistsort='Henley, Don'
albumartistsort='Various Artists'
musicbrainz_releasegroupid='08c02a79-e3a7-438d-85da-f906e111e8d6'
time=238
```

### Retrieve tags of current song:
```
file='92-folk/Woody Guthrie/Woody Guthrie -- The Asch Recordings, Volumes 1-4 (1999)/Woody Guthrie -- 04-20 - Fastest of Ponies.flac'
artist='Woody Guthrie'
album='The Asch Recordings, Volumes 1-4'
albumartist='Woody Guthrie'
title='Fastest of Ponies'
track='20'
date='1999-08-17'
disc='4'
musicbrainz_artistid='cbd827e1-4e38-427e-a436-642683433732'
musicbrainz_albumid='49f81484-0c30-4921-8ebb-3bc1478660a0'
musicbrainz_albumartistid='cbd827e1-4e38-427e-a436-642683433732'
musicbrainz_trackid='7269be97-fe2c-44d6-bde2-687dfb6f99e9'
musicbrainz_releasetrackid='359647c2-b044-3996-8409-02815b2b5266'
originaldate='1999-08-17'
artistsort='Guthrie, Woody'
albumartistsort='Guthrie, Woody'
label='Smithsonian Folkways'
musicbrainz_releasegroupid='24265bdb-727a-3241-8fd7-1b191f5269b4'
time=258
```

### Retrieve "status" information
```
$ mpdtags --status
volume: 100
repeat: 1
random: 1
single: 0
consume: 0
partition: default
playlist: 2
playlistlength: 72099
mixrampdb: -17
state: play
lastloadedplaylist: 
mixrampdelay: 5
song: 39280
songid: 39281
time: 119:229
elapsed: 119.168
bitrate: 320
duration: 228.674
audio: 44100:16:2
nextsong: 961
nextsongid: 962
```

### Retrieve tags of next song with current info:

```
$ mpdtags --next --status
file='Bob Dylan/Bob Dylan -- The Bootleg Series, Volume 07: No Direction Home - The Soundtrack (2005)/Bob Dylan -- 01-03 - This Land Is Your Land (live).flac'
artist='Bob Dylan'
album='The Bootleg Series, Volume 07:  No Direction Home - The Soundtrack'
albumartist='Bob Dylan'
title='This Land Is Your Land (live)'
track='3'
date='2010-12-13'
disc='1'
musicbrainz_artistid='72c536dc-7137-4477-a521-567eeb840fa8'
musicbrainz_albumid='45b7f41b-5a14-409a-a90a-4ec24d8c8cf7'
musicbrainz_albumartistid='72c536dc-7137-4477-a521-567eeb840fa8'
musicbrainz_trackid='bad234c5-79f9-4a05-81c3-533566da925b'
musicbrainz_releasetrackid='e63778e4-045f-415f-8dee-1c7e617fb9b7'
originaldate='2005-08-30'
artistsort='Dylan, Bob'
albumartistsort='Dylan, Bob'
label='Columbia'
label='Legacy'
musicbrainz_releasegroupid='3d1e0f14-a3c3-3107-9884-c900aa7f6a08'
time=358

volume: 100
repeat: 1
random: 1
single: 0
consume: 0
partition: default
playlist: 2
playlistlength: 72099
mixrampdb: -17
state: play
lastloadedplaylist: 
mixrampdelay: 5
song: 48751
songid: 48752
time: 241:258
elapsed: 240.938
bitrate: 375
duration: 258.200
audio: 44100:16:2
nextsong: 3353
nextsongid: 3354
```

### Retrieve tags of an **absolute path** with a defined MPD UNIX domain socket:
```
$ mpdtags /library/music/Grateful\ Dead/Grateful\ Dead\ --\ Without\ a\ Net\ \(1990\)/Grateful\ Dead\ --\ 02-01\ -\ China\ Cat\ Sunflower\ \>\ I\ Know\ You\ Rider.flac --socket=/var/run/mpd/socket 
artist='Grateful Dead'
album='Without a Net'
albumartist='Grateful Dead'
title='China Cat Sunflower / I Know You Rider'
track='1'
date='1990-09'
performer='Jerry Garcia (guitar)'
performer='Bob Weir (guitar)'
performer='Mickey Hart (drums (drum set))'
performer='Bill Kreutzmann (drums (drum set))'
performer='Phil Lesh (electric bass guitar)'
performer='Brent Mydland (keyboard)'
performer='Jerry Garcia (vocals)'
performer='Phil Lesh (vocals)'
performer='Brent Mydland (vocals)'
performer='Bob Weir (vocals)'
disc='2'
musicbrainz_artistid='6faa7ca7-0d99-4a5e-bfa6-1fd5037520c6'
musicbrainz_albumid='a9ef2528-9b35-4014-9ba1-7a44a6f81173'
musicbrainz_albumartistid='6faa7ca7-0d99-4a5e-bfa6-1fd5037520c6'
musicbrainz_trackid='ff6285b0-6c3a-49e4-89c9-33d8fa7e72ed'
musicbrainz_releasetrackid='424cafac-c9b4-3889-b79d-5151a3241f39'
originaldate='1990-09'
artistsort='Grateful Dead'
albumartistsort='Grateful Dead'
label='Arista'
musicbrainz_releasegroupid='3dbc9bfb-653c-3333-8e8a-3a12c0e7238a'
time=624
```

