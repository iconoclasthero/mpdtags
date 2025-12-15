# mpdtags

A small utility to fetch MPD song metadata, with optional socket fallback for local files.

## Usage

```
 mpdtags [options] <path>
  --host=HOST      : Specify the host URL/IP address for MPD
  --port=PORT      : Specify the host port for MPD
  --socket[=/path] : Force domain socket connection regardless of env variables**
  --local          : Specify <path> is a local file (for which TCP is prohibited)**
  --current        : Return info on the current song
  --next           : Return info on the next song
  --status         : Return the MPD status information
  --help           : Display this message

**By default MPD will use the MPD_HOST and MPD_PORT environmental variables to connect via TCP
but disallows reading of local files over TCP. mpdtags will automatically try to use a domain
socket in such a case but --local/--socket will force this with the latter providing the optional
facility to specify a socket path.
```

## Rationale

Ever wanted to use your database that MPD keeps for your music collection to retrieve metadata tags for music in your library? This allows you to use that database and thus obviates the need to keep a second, synched database of your music collection. On obvious caveat to note here is that have been intentionally excluded from the MPD database through `.mpdignore` files won't be in the database. 

MPD still has a facility to read from any local file given an absolute path, but is prohibited from doing so over TCP (e.g., from a remote connection) so a UNIX domain socket connection must be used. The `libmpdclient` attempts (and uses) the TCP connection preferentially if `MPD_HOST`/`MPD_PORT` environmental variables are set and will thus error out.

If a UNIX domain socket exists, `mpdtags` will attempt to reach the socket automatically if it recieves an error from MPD (`exception: Access to local files via TCP is not allowed`). If known in advance that mpd will be trying to retrieve an absolute path (regardless of presence in the database), `--local` or `--socket` can be used. With the latter, you can spefify a socket path as necessary: `--socket=/var/run/mpd/socket`.

## Usage examples

### Retrieve information on the last song played from the log:
```
$ mpdtags "$(last="$(grep 'player: played' /var/log/mpd/mpd.log|tail -n1)"; last="${last%\"}"; last="${last#*\"}"; echo "$last")"
file='Blackberry Smoke/Blackberry Smoke -- The Whippoorwill (2012)/Blackberry Smoke -- 01-05 - Ain'\''t Much Left of Me.flac'
Artist='Blackberry Smoke'
Album='The Whippoorwill'
AlbumArtist='Blackberry Smoke'
Title='Ain'\''t Much Left of Me'
Track='5'
Genre='Country'
Date='2012'
Disc='1'
MUSICBRAINZ_ARTISTID='1576a26e-f77d-47ac-87f2-4f610c141ac6'
MUSICBRAINZ_ALBUMID='ad78fa13-a47b-4431-a3f3-d6614c080dcb'
MUSICBRAINZ_ALBUMARTISTID='1576a26e-f77d-47ac-87f2-4f610c141ac6'
MUSICBRAINZ_TRACKID='552ad0a1-1af9-426d-84a7-2fd058c84412'
MUSICBRAINZ_RELEASETRACKID='a3ec0db0-0b5f-4b06-af8d-7acfaf6f07c7'
OriginalDate='2012-08-14'
ArtistSort='Blackberry Smoke'
AlbumArtistSort='Blackberry Smoke'
Label='Southern Ground'
MUSICBRAINZ_RELEASEGROUPID='514a7c52-82e1-41bf-b270-adefb2bd5c6c'
time=299
```

### Retrieve tags of current song:
```
$ mpdtags --current
file='Todd Snider/Todd Snider -- Live: Return of the Storyteller (2022) (mp3)/Todd Snider -- 01-05 - Play a Train Song.mp3'
Artist='Todd Snider'
Album='Live: Return of the Storyteller (mp3)'
AlbumArtist='Todd Snider'
Title='Play a Train Song'
Track='5'
Genre='Alternative Country, Americana, Folk; Americana; Country Music'
Date='2022-09-23'
Disc='1'
MUSICBRAINZ_ARTISTID='25c13ec7-fad4-4da4-8d98-9607ff615d68'
MUSICBRAINZ_ALBUMID='0fa7a8f3-d4e7-495d-a362-0d9db4afcba1'
MUSICBRAINZ_ALBUMARTISTID='25c13ec7-fad4-4da4-8d98-9607ff615d68'
MUSICBRAINZ_TRACKID='0c9848d8-fbd1-4280-b3c2-b1fc288e2b30'
MUSICBRAINZ_RELEASETRACKID='02f403f1-ec4e-4dbc-bce6-04ede53cc094'
OriginalDate='2022'
ArtistSort='Snider, Todd'
AlbumArtistSort='Snider, Todd'
Label='Aimless Records'
MUSICBRAINZ_RELEASEGROUPID='0b0b138b-f4f0-4601-8594-2983af19eaf5'
time=229
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
file='Willie Nelson/Willie Nelson -- Milk Cow Blues (2000)/Willie Nelson -- 01-06 - Crazy (feat. Susan Tedeschi).flac'
Artist='Willie Nelson'
Album='Milk Cow Blues'
AlbumArtist='Willie Nelson'
Title='Crazy (feat. Susan Tedeschi)'
Track='6'
Date='2000'
Disc='1'
MUSICBRAINZ_ARTISTID='668fd73c-bf54-4310-a139-305517f05311'
MUSICBRAINZ_ARTISTID='deabe097-2a03-49ce-9ae3-c9645bc778c7'
MUSICBRAINZ_ALBUMID='a0383301-ab5e-4f2e-b65c-5f594678916f'
MUSICBRAINZ_ALBUMARTISTID='668fd73c-bf54-4310-a139-305517f05311'
MUSICBRAINZ_TRACKID='41de9a80-105b-4e75-aac5-3008264d4978'
MUSICBRAINZ_RELEASETRACKID='331610e3-fb3e-32c7-b3fd-d76a65d6997c'
OriginalDate='2000-09-19'
ArtistSort='Nelson, Willie'
AlbumArtistSort='Nelson, Willie'
Label='Island Def Jam Music Group'
Label='Universal Music International Division'
MUSICBRAINZ_RELEASEGROUPID='fef9c612-97d7-30a9-acca-b4ab3594f633'
time=256

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
song: 961
songid: 962
time: 1:256
elapsed: 0.752
bitrate: 648
duration: 256.160
audio: 44100:16:2
nextsong: 45568
nextsongid: 45569
```

### Retrieve tags of an absolute path with a defined MPD UNIX domain socket:
```
$ mpdtags /library/music/Grateful\ Dead/Grateful\ Dead\ --\ Without\ a\ Net\ \(1990\)/Grateful\ Dead\ --\ 02-01\ -\ China\ Cat\ Sunflower\ \>\ I\ Know\ You\ Rider.flac --socket=/var/run/mpd/socket 
file='/library/music/Grateful Dead/Grateful Dead -- Without a Net (1990)/Grateful Dead -- 02-01 - China Cat Sunflower > I Know You Rider.flac'
Artist='Grateful Dead'
Album='Without a Net'
AlbumArtist='Grateful Dead'
Title='China Cat Sunflower / I Know You Rider'
Track='1'
Date='1990-09'
Performer='Jerry Garcia (guitar)'
Performer='Bob Weir (guitar)'
Performer='Mickey Hart (drums (drum set))'
Performer='Bill Kreutzmann (drums (drum set))'
Performer='Phil Lesh (electric bass guitar)'
Performer='Brent Mydland (keyboard)'
Performer='Jerry Garcia (vocals)'
Performer='Phil Lesh (vocals)'
Performer='Brent Mydland (vocals)'
Performer='Bob Weir (vocals)'
Disc='2'
MUSICBRAINZ_ARTISTID='6faa7ca7-0d99-4a5e-bfa6-1fd5037520c6'
MUSICBRAINZ_ALBUMID='a9ef2528-9b35-4014-9ba1-7a44a6f81173'
MUSICBRAINZ_ALBUMARTISTID='6faa7ca7-0d99-4a5e-bfa6-1fd5037520c6'
MUSICBRAINZ_TRACKID='ff6285b0-6c3a-49e4-89c9-33d8fa7e72ed'
MUSICBRAINZ_RELEASETRACKID='424cafac-c9b4-3889-b79d-5151a3241f39'
OriginalDate='1990-09'
ArtistSort='Grateful Dead'
AlbumArtistSort='Grateful Dead'
Label='Arista'
MUSICBRAINZ_RELEASEGROUPID='3dbc9bfb-653c-3333-8e8a-3a12c0e7238a'
time=624
```

