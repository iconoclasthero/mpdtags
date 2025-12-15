#define MPDTAGS_VERSION "0.1.1"
#include <mpd/client.h>
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
    const char *path;
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
};

///* state string */
//static const char *state_string(enum mpd_state s) {
//    switch (s) {
//        case MPD_STATE_STOP:  return "stop";
//        case MPD_STATE_PLAY:  return "play";
//        case MPD_STATE_PAUSE: return "pause";
//        default:              return "unknown";
//    }
//}

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
        } else if (!strcmp(arg, "--status")) {
            o->status = true;
        } else if (!o->path) {
            o->path = arg;
        } else {
            fprintf(stderr, "unknown argument: %s\n", arg);
            o->show_help = true;
        }
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
        "  --status         : Return the MPD status information\n"
        "  --help           : Display this message\n"
        "  --version        : Display version and exit\n"
        "\n"
        "**By default MPD will use the MPD_HOST and MPD_PORT environmental variables to connect via TCP\n"
        "but disallows reading of local files over TCP. mpdtags will automatically try to use a domain\n"
        "socket in such a case but --local/--socket will force this with the latter providing the optional\n"
        "facility to specify a socket path."
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
    parse_flags(argc, argv, &o);

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

    if (o.path) {
        /* TCP/local-file metadata fetch — handle TCP restrictions */
        if (!mpd_send_list_meta(c, o.path)) {
            fprintf(stderr, "MPD error: %s\n",
                    mpd_connection_get_error_message(c));
            goto out;
        }

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
//                        printf("%s=", mpd_tag_name(t));
//                        shellquote(v);
//                        putchar('\n');
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
//                                printf("%s=", mpd_tag_name(t));
//                                shellquote(v);
//                                putchar('\n');
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
//                printf("%s=", mpd_tag_name(t));
//                shellquote(v);
//                putchar('\n');
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
    mpd_connection_free(c);
    return 0;
}

