/*
 * Copyright 2005-2011 SPARTA, Inc.  All rights reserved.
 * See the COPYING file distributed with this software for details.
 *
 * Author: Abhijit Hayatnagarkar
 *
 * A command-line tool for testing the val_gethostbyname*() functions.
 */
#include "validator-config.h"

#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <arpa/nameser.h>
#include <validator/validator.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <netdb.h>
#include <sys/socket.h>
#include <string.h>

#include <arpa/inet.h>

#define	NAME	"gethost"
#define	VERS	"version: 1.0"
#define	DTVERS	"DNSSEC-Tools Version: 1.8"

#ifdef HAVE_GETOPT_LONG
// Program options
static struct option prog_options[] = {
    {"help", 0, 0, 'h'},
    {"novalidate", 0, 0, 'n'},
    {"family", 0, 0, 'f'},
    {"reentrant", 0, 0, 'r'},
    {"output", 0, 0, 'o'},
    {"Version", 0, 0, 'V'},
    {0, 0, 0, 0}
};
#endif

#define AUX_BUFLEN 16000

void
usage(char *progname)
{
    /* *INDENT-OFF* */
    fprintf(stderr, "Usage: %s [options] name\n", progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "\t-h, --help                      display usage and exit\n");
    fprintf(stderr, "\t-n, --novalidate                do not use the validator\n");
    fprintf(stderr, "\t-r, --reentrant                 use reentrant versions of functions\n");
    fprintf(stderr, "\t-f, --family=[AF_INET|AF_INET6] address family\n");
    fprintf(stderr, "\t                                AF_INET for IPv4 addresses,\n");
    fprintf(stderr, "\t                                and AF_INET6 for IPv6 addresses\n");
    fprintf(stderr,
            "\t-o, --output=<debug-level>:<dest-type>[:<dest-options>]\n"
            "\t          <debug-level> is 1-7, corresponding to syslog levels\n"
            "\t          <dest-type> is one of file, net, syslog, stderr, stdout\n"
            "\t          <dest-options> depends on <dest-type>\n"
            "\t              file:<file-name>   (opened in append mode)\n" 
            "\t              net[:<host-name>:<host-port>] (127.0.0.1:1053\n" 
            "\t              syslog[:facility] (0-23 (default 1 USER))\n" );
    /* *INDENT-ON* */
    fprintf(stderr, "\t-V, --Version                   display version and exit\n");
}

void
version(void)
{
    fprintf(stderr, "%s: %s\n", NAME,VERS);
    fprintf(stderr, "%s\n", DTVERS);
}

int
main(int argc, char *argv[])
{
    struct hostent  hentry;
    char            auxbuf[AUX_BUFLEN];
    struct hostent *result = NULL;

    int             i;
    val_status_t    val_status;
    int             herrno = 0;
    int             dovalidate = 1;
    int             familyspecified = 0;
    int             usereentrant = 0;
    char           *name;
    int             af = AF_INET;
    char            buf[INET6_ADDRSTRLEN];
    int             retval = 0;
    val_log_t  *logp;

    bzero(&hentry, sizeof(struct hostent));
    bzero(auxbuf, AUX_BUFLEN);

    // Parse the command line

    while (1) {
        int             c;
#ifdef HAVE_GETOPT_LONG
        int             opt_index = 0;
#ifdef HAVE_GETOPT_LONG_ONLY
        c = getopt_long_only(argc, argv, "hrnf:o:V",
                             prog_options, &opt_index);
#else
        c = getopt_long(argc, argv, "hrnf:o:V", prog_options, &opt_index);
#endif
#else                           /* only have getopt */
        c = getopt(argc, argv, "hrnf:o:V");
#endif

        if (c == -1) {
            break;
        }

        switch (c) {
        case 'h':
            usage(argv[0]);
            return -1;
        case 'n':
            dovalidate = 0;
            break;
        case 'f':
            familyspecified = 1;
            if (strcasecmp(optarg, "AF_INET") == 0) {
                af = AF_INET;
            } else if (strcasecmp(optarg, "AF_INET6") == 0) {
                af = AF_INET6;
            } else {
                fprintf(stderr, "Invalid family %s\n", optarg);
                usage(argv[0]);
                return -1;
            }
            break;
        case 'r':
            usereentrant = 1;
            break;
        case 'o':
            logp = val_log_add_optarg(optarg, 1);
            if (NULL == logp) { /* err msg already logged */
                usage(argv[0]);
                return -1;
            }
            break;
        case 'V':
            version();
            return 0;

        default:
            fprintf(stderr, "Invalid option %s\n", argv[optind - 1]);
            usage(argv[0]);
            return -1;
        }
    }


    if (optind < argc) {
        name = argv[optind++];
    } else {
        fprintf(stderr, "Error: name not specified\n");
        usage(argv[0]);
        return -1;
    }

    if (dovalidate) {
        if (usereentrant) {
#ifdef HAVE_GETHOSTBYNAME2
            if (familyspecified)
                retval =
                    val_gethostbyname2_r(NULL, name, af, &hentry, auxbuf,
                                         AUX_BUFLEN, &result, &herrno,
                                         &val_status);
            else
#endif
                retval =
                    val_gethostbyname_r(NULL, name, &hentry, auxbuf,
                                        AUX_BUFLEN, &result, &herrno,
                                        &val_status);
        } else {
            if (familyspecified)
                result = val_gethostbyname2(NULL, name, af, &val_status);
            else
                result = val_gethostbyname(NULL, name, &val_status);
        }
    } else {
        if (usereentrant) {
#if 0
            if (familyspecified)
                retval =
                    gethostbyname2_r(name, af, &hentry, auxbuf, AUX_BUFLEN,
                                     &result, &herrno);
            else
                retval = gethostbyname_r(name, &hentry, auxbuf, AUX_BUFLEN,
                                         &result, &herrno);
#endif
        } else {
#ifdef HAVE_GETHOSTBYNAME2
            if (familyspecified)
                result = gethostbyname2(name, af);
            else
#endif
                result = gethostbyname(name);
        }
    }

    if (result != NULL) {
        printf("\n\th_name = %s\n", result->h_name);
        if (result->h_aliases) {
            printf("\th_aliases = \n");
            for (i = 0; result->h_aliases[i] != 0; i++) {
                printf("\t\t[%d] = %s\n", i, result->h_aliases[i]);
            }
        } else
            printf("\th_aliases = NULL\n");
        if (result->h_addrtype == AF_INET) {
            printf("\th_addrtype = AF_INET\n");
        } else if (result->h_addrtype == AF_INET6) {
            printf("\th_addrtype = AF_INET6\n");
        } else {
            printf("\th_addrtype = %d\n", result->h_addrtype);
        }
        printf("\th_length = %d\n", result->h_length);
        printf("\th_addr_list = \n");
        for (i = 0; result->h_addr_list[i] != 0; i++) {
            bzero(buf, INET6_ADDRSTRLEN);
            printf("\t\t[%d] = %s\n", i,
                   inet_ntop(result->h_addrtype,
                             result->h_addr_list[i],
                             buf, INET6_ADDRSTRLEN));
        }
    } else {
        printf("result is NULL\n");
    }
    if (dovalidate) {
        printf("Validation status = %s\n", p_val_error(val_status));
    }
    if (usereentrant) {
#ifdef HAVE_HSTRERROR
        printf("h_errno = %s\n", hstrerror(herrno));
#else
        printf("h_errno = %d\n", herrno);
#endif
    } else {
#ifdef HAVE_HSTRERROR
        printf("h_errno = %s\n", hstrerror(h_errno));
#else
        printf("h_errno = %d\n", h_errno);
#endif
    }

    if (dovalidate) {
        if (val_isvalidated(val_status))
            return 2;
        if (val_istrusted(val_status))
            return 1;
    }

    return 0;
}
