/*
 *  Copyright(C) 2006 Cameron Rich
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "os_port.h"
#include "ssl.h"

#define BACKLOG 15
#define VERSION "1.0.0"
#ifdef CONFIG_HTTP_HAS_IPV6
#define HAVE_IPV6
#endif

#define MAXFILEPATH 1024
#define MAXIPLEN 45
#define MAXREQUESTLENGTH 1024
#define MAXCGIARGS 100
#define BLOCKSIZE 4096

#define INITIAL_CONNECTION_SLOTS 10
#define CONFIG_HTTP_DEFAULT_SSL_OPTIONS 0

#define STATE_WANT_TO_READ_HEAD  1
#define STATE_WANT_TO_SEND_HEAD  2
#define STATE_WANT_TO_READ_FILE  3
#define STATE_WANT_TO_SEND_FILE  4
#define STATE_DOING_DIR          5

#define TYPE_GET 0
#define TYPE_HEAD 1
#define TYPE_POST 2

struct connstruct 
{
    struct connstruct *next;
    int state;
    int reqtype;
    int networkdesc;
    int filedesc;

#if defined(CONFIG_HTTP_DIRECTORIES)
#ifdef WIN32
    HANDLE dirp;
    WIN32_FIND_DATA file_data;
#else
    DIR *dirp;
#endif
#endif

    time_t timeout;
    char ip[MAXIPLEN];
    char actualfile[MAXREQUESTLENGTH];
    char filereq[MAXREQUESTLENGTH];
#if defined(CONFIG_HTTP_HAS_CGI)
    char cgiargs[MAXREQUESTLENGTH];
    char cgiscriptinfo[MAXREQUESTLENGTH];
    char cgipathinfo[MAXREQUESTLENGTH];
#endif
    char virtualhostreq[MAXREQUESTLENGTH];
    int numbytes;
    char databuf[BLOCKSIZE];

    uint8_t is_ssl;
    uint8_t close_when_done;
    uint8_t modified_since;
};

struct serverstruct 
{
    struct serverstruct *next;
    int sd;
    int is_ssl;
    SSLCTX *ssl_ctx;
};

#if defined(CONFIG_HTTP_HAS_CGI)
struct cgiextstruct 
{
    struct cgiextstruct *next;
    char *ext;
};
#endif

// Global prototypes
extern struct serverstruct *servers;
extern struct connstruct *usedconns;
extern struct connstruct *freeconns;
#if defined(CONFIG_HTTP_HAS_CGI)
extern struct cgiextstruct *cgiexts;
#endif

// Conf global prototypes
extern char *webroot;

// conn.c prototypes
void addconnection(int sd, char *ip, int is_ssl);
void removeconnection(struct connstruct *cn);

// proc.c prototypes
void procdodir(struct connstruct *cn);
void procreadhead(struct connstruct *cn);
void procsendhead(struct connstruct *cn);
void procreadfile(struct connstruct *cn);
void procsendfile(struct connstruct *cn);

// misc.c prototypes
char *my_strncpy(char *dest, const char *src, size_t n);
int isdir(const char *name);

// mime_types.c prototypes
void mime_init(void);
const char *getmimetype(const char *fn);