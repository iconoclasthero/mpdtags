#define LIBRARY_PATH "/library/music"
#define MPDTAGS_VERSION "0.1.5"
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

static const char *abs_path_prefix = "/library/music";

/* lowercase copy of a string; caller must free */
char *strtolower(const char *s) {
    char *res = strdup(s);
    if (!res) return NULL;
    for (char *p = res; *p; p++)
        *p = tolower((unsigned char)*p);
    return res;
}

char *unescape_mpd_path(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    char *d = out;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\\' && (s[i+1] == '\\' || s[i+1] == '"'))
            i++;
        *d++ = s[i];
    }
    *d = '\0';
    return out;
}

/* stub DB lookup: returns 0 on success */
static int mpdtags_lookup(const char *relpath) { return 1; }

/* stub socket lookup: returns 0 on success */
static int mpdtags_lookup_socket(const char *abs_path) { return 1; }

/* find last played from log */
static char *find_last_played(const char *logpath, const struct opts *o) {
    FILE *fp = fopen(logpath, "r");
    if (!fp) return NULL;

    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    long pos = ftell(fp);
    if (pos <= 0) { fclose(fp); return NULL; }

    char buf[8192];
    size_t idx = 0;

    while (pos > 0) {
        pos--;
        if (fseek(fp, pos, SEEK_SET) != 0) break;

        int c = fgetc(fp);
        if (c == '\n' || pos == 0) {
            if (idx == 0) continue;
            buf[idx] = '\0';
            idx = 0;

            /* reverse buffer */
            for (size_t i = 0, j = strlen(buf)-1; i < j; i++, j--) {
                char tmp = buf[i];
                buf[i] = buf[j];
                buf[j] = tmp;
            }

            const char *matched_state = NULL;
            for (int i = 0; i < o->nstates; i++) {
                char needle[64];
                snprintf(needle, sizeof(needle), "player: %s ", o->player_states[i]);
                if (strstr(buf, needle)) { matched_state = o->player_states[i]; break; }
            }
            if (!matched_state) continue;

            char *start = strchr(buf, '"');
            char *end   = start ? strrchr(start+1, '"') : NULL;
            if (!start || !end || end <= start+1) continue;

            *end = '\0';
            char *rel_path = unescape_mpd_path(start+1);

            char *lastcompleted = NULL;
            char *sep = strstr(buf, " player: ");
            if (sep) { *sep = '\0'; lastcompleted = strdup(buf); }

            if (lastcompleted) printf("completed=%s\n", lastcompleted);
            printf("player=%s\n", matched_state);

            if (mpdtags_lookup(rel_path) == 0) {
                free(lastcompleted);
                fclose(fp);
                return rel_path;
            }

            char abs_path[PATH_MAX];
            snprintf(abs_path, sizeof(abs_path), "%s/%s", abs_path_prefix, rel_path);
            char *abs_dup = strdup(abs_path);

            if (mpdtags_lookup_socket(abs_dup) == 0) {
                free(rel_path);
                free(lastcompleted);
                fclose(fp);
                return abs_dup;
            }

            free(abs_dup);
            free(lastcompleted);
            fclose(fp);
            return rel_path;
        }

        if (idx < sizeof(buf)-1) buf[idx++] = (char)c;
    }

    fclose(fp);
    return NULL;
}

/* resolve socket path */
static const char *resolve_socket(void) {
    const char *s = getenv("MPD_SOCKET");
    if (s && *s) return s;

    const char *xdg = getenv("XDG_RUNTIME_DIR");
    static char buf[512];
    if (xdg) {
        snprintf(buf, sizeof buf, "%s/mpd/socket", xdg);
        if (access(buf, R_OK|W_OK)==0) return buf;
    }
    const char *home = getenv("HOME");
    if (home) {
        snprintf(buf, sizeof buf, "%s/.mpd/socket", home);
        if (access(buf, R_OK|W_OK)==0) return buf;
    }
    if (access("/run/mpd/socket", R_OK|W_OK)==0) return "/run/mpd/socket";

    return NULL;
}

/* parse command-line flags */
static void parse_flags(int argc, char **argv, struct opts *o) {
    if (argc == 1) { o->show_help = true; return; }

    for (int i=1;i<argc;i++) {
        char *arg = argv[i];

        if (!strncmp(arg,"--host=",7)) o->host=arg+7;
        else if (!strncmp(arg,"--port=",7)) o->port=(unsigned)atoi(arg+7);
        else if (!strncmp(arg,"--socket=",9)) { o->use_socket=true; o->socket_path=arg+9; }
        else if (!strcmp(arg,"--socket")) o->use_socket=true;
        else if (!strncmp(arg,"--player=",9)) {
            if (o->nstates<MAX_PLAYER_STATES) o->player_states[o->nstates++]=arg+9;
            else fprintf(stderr,"mpdtags: too many --player options (max %d)\n",MAX_PLAYER_STATES);
        }
        else if (!strcmp(arg,"--local")) o->local=true;
        else if (!strcmp(arg,"--help")) o->show_help=true;
        else if (!strcmp(arg,"--version")) o->show_version=true;
        else if (!strcmp(arg,"--current")) { o->current=true; o->path=NULL; }
        else if (!strcmp(arg,"--next")) { o->next=true; o->path=NULL; }
        else if (!strncmp(arg,"--last",6)) {
            o->last=true;
            if (argv[i][6]=='=') o->logpath=argv[i]+7; else o->logpath=NULL;
        }
        else if (!strcmp(arg,"--status")) o->status=true;
        else if (!o->path) o->path=arg;
        else { fprintf(stderr,"unknown argument: %s\n",arg); o->show_help=true; }
    }

    if (o->nstates==0) o->player_states[o->nstates++]="played";
}

/* help */
static void print_help(void) {
    puts(
        "usage: mpdtags [options] <path>\n"
        "  --host=HOST      : MPD host\n"
        "  --port=PORT      : MPD port\n"
        "  --socket[=/path] : domain socket\n"
        "  --local          : local file\n"
        "  --current        : current song\n"
        "  --next           : next song\n"
        "  --last[=/path]   : last song from log\n"
        "  --status         : MPD status\n"
        "  --help           : help\n"
        "  --version        : version\n"
    );
}

/* connect to MPD */
static struct mpd_connection *connect_mpd(struct opts *o) {
    struct mpd_connection *c;
    if (o->use_socket || o->local) {
        const char *sock = o->socket_path ? o->socket_path : resolve_socket();
        if (!sock) { fprintf(stderr,"no MPD socket available\n"); return NULL; }
        c = mpd_connection_new(sock,0,30000);
    } else if (o->host) {
        unsigned port = o->port ? o->port:6600;
        c = mpd_connection_new(o->host,port,30000);
        if (o->local) { fprintf(stderr,"--local not valid with TCP\n"); mpd_connection_free(c); return NULL; }
    } else c = mpd_connection_new(NULL,0,30000);

    if (!c || mpd_connection_get_error(c)!=MPD_ERROR_SUCCESS) {
        fprintf(stderr,"MPD connect error: %s\n",mpd_connection_get_error_message(c));
        if (c) mpd_connection_free(c);
        return NULL;
    }
    return c;
}

/* print tags for a song */
static void print_song(struct mpd_song *s) {
    printf("file="); shellquote(mpd_song_get_uri(s)); putchar('\n');

    for (int t=0;t<MPD_TAG_COUNT;t++) {
        const char *v;
        for (unsigned i=0;(v=mpd_song_get_tag(s,t,i));i++) {
            char *tag = strtolower(mpd_tag_name(t));
            if (!tag) continue;
            printf("%s=", tag);
            shellquote(v); putchar('\n'); free(tag);
        }
    }

    int len = mpd_song_get_duration(s);
    if (len>0) printf("time=%d\n",len);
    putchar('\n');
}

int main(int argc, char **argv) {
    struct opts o = {0};
    bool path_alloc=false;

    parse_flags(argc,argv,&o);

    if (o.last) {
        const char *logpath = o.logpath ? o.logpath : getenv("MPD_LOG") ? getenv("MPD_LOG") : DEFAULT_MPD_LOG;
        char *p = find_last_played(logpath,&o);
        if (!p) { fprintf(stderr,"mpdtags: unable to determine last played song from %s\n",logpath); return 1; }
        o.path = p; path_alloc=true;
    }

    if (o.show_help) { print_help(); return 0; }
    if (o.show_version) { printf("mpdtags %s\n",MPDTAGS_VERSION); return 0; }

    struct mpd_connection *c = connect_mpd(&o);
    if (!c) return 1;

    struct mpd_status *st=NULL;
    struct mpd_song *song=NULL;

    if (o.status || o.next) {
        st = mpd_run_status(c);
        if (!st) { fprintf(stderr,"MPD status error: %s\n",mpd_connection_get_error_message(c)); goto out; }
    }

    if (o.current) {
        song = mpd_run_current_song(c);
        if (!song) { fprintf(stderr,"no current song\n"); goto out; }
    }

    if (o.next) {
        unsigned nextid = mpd_status_get_next_song_id(st);
        if (nextid==0) { fprintf(stderr,"no next song\n"); goto out; }
        song = mpd_run_get_queue_song_id(c,nextid);
        if (!song) { fprintf(stderr,"failed to get next song for id %u\n",nextid); goto out; }
    }

    /* handle local file for --last */
    if (o.last && o.use_socket && o.path[0]!='/') {
        size_t len=strlen(abs_path_prefix)+1+strlen(o.path)+1;
        char *abs=malloc(len);
        if (!abs) { fprintf(stderr,"out of memory\n"); goto out; }
        snprintf(abs,len,"%s/%s",abs_path_prefix,o.path);
        if (path_alloc) free((char *)o.path);
        o.path=abs; path_alloc=true;
        o.local=true;
    }

//    if (o.path) {
//        if (!mpd_send_list_meta(c,o.path)) {
//            fprintf(stderr,"MPD error: %s\n",mpd_connection_get_error_message(c));
//            goto out;
//        }
//
//        struct mpd_entity *ent;
//        while ((ent=mpd_recv_entity(c))) {
//            if (mpd_entity_get_type(ent)==MPD_ENTITY_TYPE_SONG)
//                print_song((struct mpd_song*)mpd_entity_get_song(ent));
//            mpd_entity_free(ent);
//        }
//        mpd_response_finish(c);
//    }
if (o.path) {
    if (!o.use_socket && !o.local && o.path[0]=='/') {
        fprintf(stderr,"\nMPD cannot access local file over TCP:\n'%s'\n\nTry using --local or --socket\n\n", o.path);
        goto out;
    }

    if (!mpd_send_list_meta(c,o.path)) {
        fprintf(stderr,"MPD error: %s\n", mpd_connection_get_error_message(c));
        goto out;
    }

    struct mpd_entity *ent;
    while ((ent = mpd_recv_entity(c))) {
        if (mpd_entity_get_type(ent) == MPD_ENTITY_TYPE_SONG)
            print_song((struct mpd_song*)mpd_entity_get_song(ent));
        mpd_entity_free(ent);
    }
    mpd_response_finish(c);
}

    if (song) print_song(song);

    if (o.status) {
        if (!mpd_send_command(c,"status",NULL)) { fprintf(stderr,"send failed\n"); goto out; }
        struct mpd_pair *pair;
        while ((pair=mpd_recv_pair(c))!=NULL) { printf("%s: %s\n",pair->name,pair->value); mpd_return_pair(c,pair); }
        if (!mpd_response_finish(c)) { fprintf(stderr,"response error\n"); goto out; }
    }

out:
    if (song) mpd_song_free(song);
    if (st) mpd_status_free(st);
    if (path_alloc && o.path) free((char*)o.path);
    mpd_connection_free(c);
    return 0;
}
