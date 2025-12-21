#define LIBRARY_PATH "/library/music"
#define MPDTAGS_VERSION "0.1.4"
#define DEFAULT_MPD_LOG "/var/log/mpd/mpd.log"
#define MAX_PLAYER_STATES 8
#include <mpd/client.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>

/* minimal shellquote */
static void shellquote(const char *s) {
    putchar('\'');
    for (; *s; s++) {
        if (*s == '\'')
            fputs("'\\''", stdout);
        else
            putchar(*s);
    }
    putchar('\'');
}

/* opts */
struct opts {
//    const char *path;
    char *path;
    const char *host;
    unsigned port;
    bool use_socket;
    const char *socket_path;
    bool local;
    bool show_help;
    bool current;
    bool next;
    bool status;
    bool show_version;
    bool last;
    const char *logpath;
    const char *player_states[MAX_PLAYER_STATES];
    int nstates;
};


//const char *abs_path_prefix = "/library/music";
static const char *abs_path_prefix = "/library/music";

///* state string */
//static const char *state_string(enum mpd_state s) {
//    switch (s) {
//        case MPD_STATE_STOP:  return "stop";
//        case MPD_STATE_PLAY:  return "play";
//        case MPD_STATE_PAUSE: return "pause";
//        default:              return "unknown";
//    }
//}


char *unescape_mpd_path(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = malloc(len + 1);  // worst case
    if (!out) return NULL;
    char *d = out;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\\' && (s[i+1] == '\\' || s[i+1] == '"')) {
            i++;
        }
        *d++ = s[i];
    }
    *d = '\0';
    return out;
}

// returns 0 on success, non-zero on failure
static int mpdtags_lookup(const char *relpath) {
    // TODO: implement DB lookup
    return 1; // simulate "not found"
}

static int mpdtags_lookup_socket(const char *abs_path) {
    // TODO: implement socket lookup
    return 1; // simulate "not found"
}

static char *find_last_played(const char *logpath, const struct opts *o)
{
    FILE *fp = fopen(logpath, "r");
    if (!fp)
        return NULL;

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }

    long pos = ftell(fp);
    if (pos <= 0) {
        fclose(fp);
        return NULL;
    }

    char buf[8192];
    size_t idx = 0;

    while (pos > 0) {
        pos--;
        if (fseek(fp, pos, SEEK_SET) != 0)
            break;

        int c = fgetc(fp);
        if (c == '\n' || pos == 0) {
            if (idx == 0)
                continue;

            buf[idx] = '\0';
            idx = 0;

            /* reverse buffer into line */
            for (size_t i = 0, j = strlen(buf) - 1; i < j; i++, j--) {
                char tmp = buf[i];
                buf[i] = buf[j];
                buf[j] = tmp;
            }

            /* original logic kept */
            // if (strstr(buf, "player: played \"")) {
            //     ...
            // }

            /* match requested player states */
            const char *matched_state = NULL;

            for (int i = 0; i < o->nstates; i++) {
                char needle[64];
                snprintf(needle, sizeof(needle),
                         "player: %s ", o->player_states[i]);

                if (strstr(buf, needle)) {
                    matched_state = o->player_states[i];
                    break;
                }
            }

            if (!matched_state)
                continue;

            /* extract quoted path */
            char *start = strchr(buf, '"');
            char *end   = start ? strrchr(start + 1, '"') : NULL;
            if (!start || !end || end <= start + 1)
                continue;

            *end = '\0';
            char *rel_path = unescape_mpd_path(start + 1);

            /* extract timestamp */
            char *lastcompleted = NULL;
            char *sep = strstr(buf, " player: ");
            if (sep) {
                *sep = '\0';
                lastcompleted = strdup(buf);
            }

            /* log-derived output happens FIRST */
            if (lastcompleted)
                printf("completed=%s\n", lastcompleted);
            printf("player=%s\n", matched_state);

            /* try DB lookup first (relative path) */
            if (mpdtags_lookup(rel_path) == 0) {
                free(lastcompleted);
                fclose(fp);
                return rel_path;
            }

            /* fallback: absolute path via socket */
            {
                char abs_path[PATH_MAX];
                snprintf(abs_path, sizeof(abs_path),
                         "%s/%s", abs_path_prefix, rel_path);

                char *abs_dup = strdup(abs_path);

                if (mpdtags_lookup_socket(abs_dup) == 0) {
                    free(rel_path);
                    free(lastcompleted);
                    fclose(fp);
                    return abs_dup;
                }

                /* both failed: still return the log path */
                free(abs_dup);
            }

            free(lastcompleted);
            fclose(fp);
            return rel_path;
        }

        if (idx < sizeof(buf) - 1)
            buf[idx++] = (char)c;
    }

    fclose(fp);
    return NULL;
}

/* resolve socket */
static const char *resolve_socket(void) {
    const char *s = getenv("MPD_SOCKET");
    if (s && *s) return s;

    const char *xdg = getenv("XDG_RUNTIME_DIR");
    static char buf[512];
    if (xdg) {
        snprintf(buf, sizeof buf, "%s/mpd/socket", xdg);
        if (access(buf, R_OK | W_OK) == 0) return buf;
    }

    const char *home = getenv("HOME");
    if (home) {
        snprintf(buf, sizeof buf, "%s/.mpd/socket", home);
        if (access(buf, R_OK | W_OK) == 0) return buf;
    }

    if (access("/run/mpd/socket", R_OK | W_OK) == 0)
        return "/run/mpd/socket";

    return NULL;
}

/* parse flags */
static void parse_flags(int argc, char **argv, struct opts *o) {

    if (argc == 1) {  // no arguments
        o->show_help = true;
        return;        // nothing else to parse
    }

    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];

        if (!strncmp(arg, "--host=", 7))
            o->host = arg + 7;
        else if (!strncmp(arg, "--port=", 7))
            o->port = (unsigned)atoi(arg + 7);
        else if (!strncmp(arg, "--socket=", 9)) {
            o->use_socket = true;
            o->socket_path = arg + 9;
        } else if (!strcmp(arg, "--socket")) {
            o->use_socket = true;
				} else if (!strncmp(arg, "--player=", 9)) {
				    if (o->nstates < MAX_PLAYER_STATES) {
				        o->player_states[o->nstates++] = arg + 9;  // points into argv
				    } else {
				        fprintf(stderr, "mpdtags: too many --player options (max %d)\n",
				                MAX_PLAYER_STATES);
				    }
        } else if (!strcmp(arg, "--local")) {
            o->local = true;
        } else if (!strcmp(arg, "--help")) {
            o->show_help = true;
        } else if (!strcmp(arg, "--version")) {
            o->show_version = true;
        } else if (!strcmp(arg, "--current")) {
            o->current = true;
            o->path = NULL;
        } else if (!strcmp(arg, "--next")) {
            o->next = true;
            o->path = NULL;
				} else if (strncmp(argv[i], "--last", 6) == 0) {
				    o->last = true;
				    if (argv[i][6] == '=') {          // --last=/path/to/log
				        o->logpath = argv[i] + 7;     // skip "--last="
				    } else {
				        o->logpath = NULL;            // will use default later
				    }
        } else if (!strcmp(arg, "--status")) {
            o->status = true;
        } else if (!o->path) {
            o->path = arg;
        } else {
            fprintf(stderr, "unknown argument: %s\n", arg);
            o->show_help = true;
        }
    }

		if (o->nstates == 0) {
		    o->player_states[o->nstates++] = "played";
		}
}


/* help */
static void print_help(void) {
    puts(
        "usage: mpdtags [options] <path>\n"
        "  --host=HOST      : Specify the host URL/IP address for MPD\n"
        "  --port=PORT      : Specify the host port for MPD\n"
        "  --socket[=/path] : Force domain socket connection regardless of env variables**\n"
        "  --local          : Specify <path> is a local file (for which TCP is prohibited)**\n"
        "  --current        : Return info on the current song\n"
        "  --next           : Return info on the next song\n"
        "  --last[=/path]   : Return info on the last song in log; defaults to /var/log/mpd/mpd.log\n"
        "                   : Use --last=/path/to/mpd.log or set MPD_LOG variable for alternate log path\n"
        "  --status         : Return the MPD status information\n"
        "  --help           : Display this message\n"
        "  --version        : Display version and exit\n"
        "\n"
        "**By default MPD will use the MPD_HOST and MPD_PORT environmental variables to connect via TCP\n"
        "but disallows reading of local files over TCP. mpdtags will automatically try to use a domain\n"
        "socket in such a case but --local/--socket will force this with the latter providing the optional\n"
        "facility to specify a socket path.\n"
        "--last cannot be used remotely\n"
    );
}

/* connect */
static struct mpd_connection *connect_mpd(struct opts *o) {
    struct mpd_connection *c;

    if (o->use_socket || o->local) {
        const char *sock = o->socket_path ? o->socket_path : resolve_socket();
        if (!sock) {
            fprintf(stderr, "no MPD socket available\n");
            return NULL;
        }
        c = mpd_connection_new(sock, 0, 30000);
    } else if (o->host) {
        unsigned port = o->port ? o->port : 6600;
        c = mpd_connection_new(o->host, port, 30000);
        if (o->local) {
            fprintf(stderr, "--local not valid with TCP\n");
            mpd_connection_free(c);
            return NULL;
        }
    } else {
        c = mpd_connection_new(NULL, 0, 30000);
    }

    if (!c || mpd_connection_get_error(c) != MPD_ERROR_SUCCESS) {
        fprintf(stderr, "MPD connect error: %s\n",
                mpd_connection_get_error_message(c));
        if (c) mpd_connection_free(c);
        return NULL;
    }

    return c;
}

/* return a lowercase copy of a string; caller must free */
char *strtolower(const char *s) {
    char *res = strdup(s);
    if (!res) return NULL;
    for (char *p = res; *p; p++)
        *p = tolower((unsigned char)*p);
    return res;
}


int main(int argc, char **argv) {
    struct opts o = {0};
    bool path_is_allocated = false;
    parse_flags(argc, argv, &o);


		if (o.last) {
		    const char *logpath =
		        o.logpath ? o.logpath : getenv("MPD_LOG") ? getenv("MPD_LOG") : DEFAULT_MPD_LOG;

		    char *p = find_last_played(logpath, &o);
		    if (!p) {
		        fprintf(stderr, "mpdtags: unable to determine last played song from %s\n", logpath);
		        return 1;
		    }
//		    o.path = p;
o.path = p;
path_is_allocated = true;

		}

    if (o.show_help) {
        print_help();
        return 0;
    }

    if (o.show_version) {
        printf("mpdtags %s\n", MPDTAGS_VERSION);
        return 0;
    }

    struct mpd_connection *c = connect_mpd(&o);
    if (!c) return 1;

    struct mpd_status *st = NULL;
    struct mpd_song *song = NULL;

    /* status needed */
    if (o.status || o.next) {
        st = mpd_run_status(c);
        if (!st) {
            fprintf(stderr, "MPD status error: %s\n",
                    mpd_connection_get_error_message(c));
            goto out;
        }
    }

    /* resolve song */
    if (o.current) {
        song = mpd_run_current_song(c);
        if (!song) {
            unsigned err = mpd_connection_get_error(c);
            if (err != MPD_ERROR_SUCCESS)
                fprintf(stderr, "MPD error fetching current song: %s\n",
                        mpd_connection_get_error_message(c));
            else
                fprintf(stderr, "no current song\n");
            goto out;
        }
    }

    if (o.next) {
        if (!st) {
            st = mpd_run_status(c);
            if (!st) {
                fprintf(stderr, "status error: %s\n",
                        mpd_connection_get_error_message(c));
                goto out;
            }
        }

        unsigned nextid = mpd_status_get_next_song_id(st);
        if (nextid == 0) {
            fprintf(stderr, "no next song\n");
            goto out;
        }

        song = mpd_run_get_queue_song_id(c, nextid);
        if (!song) {
            unsigned err = mpd_connection_get_error(c);
            if (err != MPD_ERROR_SUCCESS)
                fprintf(stderr, "MPD error fetching next song: %s\n",
                        mpd_connection_get_error_message(c));
            else
                fprintf(stderr, "failed to get next song for id %u\n", nextid);
            goto out;
        }
    }

/* --last ignored files must be handled as local files */
if (o.last && o.use_socket) {
    /* absolute path required for local lookup */
    if (o.path[0] != '/') {
        size_t len = strlen(abs_path_prefix) + 1 + strlen(o.path) + 1;
        char *abs = malloc(len);
        if (!abs) {
            fprintf(stderr, "out of memory\n");
            goto out;
        }

        snprintf(abs, len, "%s/%s", abs_path_prefix, o.path);

        if (path_is_allocated)
            free((char *)o.path);

        o.path = abs;
        path_is_allocated = true;
    }

    /* force local-file semantics */
    o.local = true;
}


    if (o.path) {
        /* TCP/local-file metadata fetch — handle TCP restrictions */
//        if (!mpd_send_list_meta(c, o.path)) {
//            fprintf(stderr, "MPD error: %s\n",
//                    mpd_connection_get_error_message(c));
//            goto out;
//        }
if (!mpd_send_list_meta(c, o.path)) {

    /* --last fallback: retry via absolute path over socket */
    if (o.last && o.use_socket) {
        char *abs_path;
        int len = strlen(abs_path_prefix) + 1 + strlen(o.path) + 1;

        abs_path = malloc(len);
        if (abs_path) {
            snprintf(abs_path, len, "%s/%s", abs_path_prefix, o.path);

//            mpd_connection_clear_error(c);
//
//            if (mpd_send_list_meta(c, abs_path)) {
//                free(o.path);
//                o.path = abs_path;
//                goto read_tags;
//            }
mpd_connection_free(c);

c = connect_mpd(&(struct opts){
    .use_socket = true,
    .socket_path = o.socket_path,
    .local = true
});

if (c && mpd_send_list_meta(c, abs_path)) {
    free(o.path);
    o.path = abs_path;
    goto read_tags;
}

            free(abs_path);
        }
    }

    fprintf(stderr, "MPD error: %s\n",
            mpd_connection_get_error_message(c));
    goto out;
}

read_tags:

        struct mpd_entity *ent;
        while ((ent = mpd_recv_entity(c))) {
            if (mpd_entity_get_type(ent) == MPD_ENTITY_TYPE_SONG) {
                struct mpd_song *s = (struct mpd_song *)mpd_entity_get_song(ent);

                printf("file=");
                shellquote(mpd_song_get_uri(s));
                putchar('\n');

                for (int t = 0; t < MPD_TAG_COUNT; t++) {
                    const char *v;
                    for (unsigned i = 0; (v = mpd_song_get_tag(s, t, i)); i++) {
                        char *tag = strtolower(mpd_tag_name(t));   // lowercase copy
                        if (!tag) continue;                        // safety
                        printf("%s=", tag);                        // use lowercase
                        shellquote(v);                             // shellquote RHS
                        putchar('\n');                             // new line
                        free(tag);                                 // free memory
                    }
                }

                int len = mpd_song_get_duration(s);
                if (len > 0)
                    printf("time=%d\n", len);

                putchar('\n');
            }
            mpd_entity_free(ent);
        }

        mpd_response_finish(c);

        /* Handle TCP local-file errors and retry with socket */
        if (mpd_connection_get_error(c) != MPD_ERROR_SUCCESS) {
            const char *msg = mpd_connection_get_error_message(c);

            if (!o.use_socket && strstr(msg, "local files") && strstr(msg, "TCP")) {
                fprintf(stderr, "MPD error: %s; trying socket instead...\n\n", msg);

                mpd_connection_free(c);
                o.use_socket = true;
                o.host = NULL;
                c = connect_mpd(&o);
                if (!c) {
                    fprintf(stderr, "MPD error: failed to connect via socket\n");
                    return 1;
                }

                /* retry exactly once */
                if (!mpd_send_list_meta(c, o.path)) {
                    fprintf(stderr, "MPD error: %s\n",
                            mpd_connection_get_error_message(c));
                    goto out;
                }

                while ((ent = mpd_recv_entity(c))) {
                    if (mpd_entity_get_type(ent) == MPD_ENTITY_TYPE_SONG) {
                        struct mpd_song *s = (struct mpd_song *)mpd_entity_get_song(ent);

                        printf("file=");
                        shellquote(mpd_song_get_uri(s));
                        putchar('\n');

                        for (int t = 0; t < MPD_TAG_COUNT; t++) {
                            const char *v;
                            for (unsigned i = 0; (v = mpd_song_get_tag(s, t, i)); i++) {
                                char *tag = strtolower(mpd_tag_name(t));   // lowercase copy
                                if (!tag) continue;                        // safety
                                printf("%s=", tag);                        // use lowercase
                                shellquote(v);                             // shellquote RHS
                                putchar('\n');                             // new line
                                free(tag);                                 // free memory
                            }
                        }

                        int len = mpd_song_get_duration(s);
                        if (len > 0)
                            printf("time=%d\n", len);

                        putchar('\n');
                    }
                    mpd_entity_free(ent);
                }

                mpd_response_finish(c);

                /* if --status was requested, fetch and print it */
                if (o.status) {
                    struct mpd_status *st2 = mpd_run_status(c);
                    if (!st2) {
                        fprintf(stderr, "MPD status error: %s\n",
                                mpd_connection_get_error_message(c));
                    } else {
                        struct mpd_pair *pair;
                        while ((pair = mpd_recv_pair(c)) != NULL) {
                            printf("%s: %s\n", pair->name, pair->value);
                            mpd_return_pair(c, pair);
                        }
                        if (!mpd_response_finish(c)) {
                            fprintf(stderr, "MPD status response error\n");
                        }
                        mpd_status_free(st2);
                    }
                }

                if (mpd_connection_get_error(c) != MPD_ERROR_SUCCESS) {
                    fprintf(stderr, "MPD error: %s\n",
                            mpd_connection_get_error_message(c));
                    goto out;
                }

                goto out;
            }

            /* fallback: normal error */
            fprintf(stderr, "MPD error: %s\n", msg);
            goto out;
        }
    }

    /* single song output */
    if (song) {
        printf("file=");
        shellquote(mpd_song_get_uri(song));
        putchar('\n');

        for (int t = 0; t < MPD_TAG_COUNT; t++) {
            const char *v;
            for (unsigned i = 0; (v = mpd_song_get_tag(song, t, i)); i++) {
                char *tag = strtolower(mpd_tag_name(t));   // lowercase copy
                if (!tag) continue;                        // safety
                printf("%s=", tag);                        // use lowercase
                shellquote(v);                             // shellquote RHS
                putchar('\n');                             // new line
                free(tag);                                 // free memory
            }
        }

        int len = mpd_song_get_duration(song);
        if (len > 0)
            printf("time=%d\n", len);

        putchar('\n');
    }

    /* status output */
    if (o.status) {
        if (!mpd_send_command(c, "status", NULL)) {
            fprintf(stderr, "send failed\n");
            goto out;
        }

        struct mpd_pair *pair;
        while ((pair = mpd_recv_pair(c)) != NULL) {
            printf("%s: %s\n", pair->name, pair->value);
            mpd_return_pair(c, pair);
        }

        if (!mpd_response_finish(c)) {
            fprintf(stderr, "response error\n");
            goto out;
        }
    }

	out:
	    if (song) mpd_song_free(song);
	    if (st) mpd_status_free(st);

	    /* free path only if we allocated it via --last */
//	    if (o.last && o.path)
//	        free((char *)o.path);
if (path_is_allocated && o.path)
    free((char *)o.path);

	    mpd_connection_free(c);
	    return 0;
}

