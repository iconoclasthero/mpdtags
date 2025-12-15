# mpdtags

A small utility to fetch MPD song metadata, with optional socket fallback for local files.

## Usage

 `mpdtags [options] <path>`
 - `--host=HOST`      : Specify the host URL/IP address for MPD
 - `--port=PORT`      : Specify the host port for MPD
 - `--socket[=/path]` : Force domain socket connection regardless of env variables**
 - `--local`          : Specify <path> is a local file (for which TCP is prohibited)**
 - `--current`        : Return info on the current song
 - `--next`           : Return info on the next song
 - `--status`         : Return the MPD status information
 - `--help`           : Display this message

