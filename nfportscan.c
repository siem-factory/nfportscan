#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/queue.h>

#include "file.h"
#include "list.h"

#define PROTO_TCP 6
#define PROTO_UDP 17
#define IPV4_ADDR_STR_LEN_MAX 20
#define INCIDENT_LIST_INITIAL 10
#define INCIDENT_LIST_EXPAND 10

/* global options */
typedef struct {
    unsigned int verbose;
    unsigned int threshhold;
} options_t;

options_t opts;

static void print_help(FILE *output)
{
    fprintf(output, "USAGE: nfportscan [OPTIONS] FILE [FILE] ...\n"
                    "  -t    --threshhold   set flow minimum for an ip address to be reported\n"
                    "                       (default: 100)\n"
                    "  -v    --verbose      set verbosity level\n"
                    "  -h    --help         print this help\n");
}

static int process_flow(master_record_t *mrec, incident_list_t **list)
{
    incident_list_t *l = *list;
    /* count global flows */
    l->flows++;

    /* throw away everything except TCP or UDP IPv4 flows */
    //if ( (mrec->prot != PROTO_TCP && mrec->prot != PROTO_UDP)
    //       || mrec->flags & FLAG_IPV6_ADDR)
    //   return 0;
    //if ( mrec->prot != PROTO_TCP || mrec->flags & FLAG_IPV6_ADDR)
    //    return 0;
    if ( (mrec->prot != PROTO_TCP && mrec->prot != PROTO_UDP)
                || mrec->flags & FLAG_IPV6_ADDR)
        return 0;

    if ( mrec->dstport == 80 )
        return 0;

    /* count flows */
    l->incident_flows++;

    if (opts.verbose >= 4) {
        char src[IPV4_ADDR_STR_LEN_MAX], dst[IPV4_ADDR_STR_LEN_MAX];

        /* convert source and destination ip to network byte order */
        mrec->v4.srcaddr = htonl(mrec->v4.srcaddr);
        mrec->v4.dstaddr = htonl(mrec->v4.dstaddr);

        /* make strings from ips */
        inet_ntop(AF_INET, &mrec->v4.srcaddr, src, sizeof(src));
        inet_ntop(AF_INET, &mrec->v4.dstaddr, dst, sizeof(dst));

        printf("incident flow: %s: %d -> %s: %d\n", src, mrec->srcport, dst, mrec->dstport);
    }

    list_insert(list, mrec->v4.srcaddr, mrec->dstport, mrec->v4.dstaddr);
    return 1;
}

static int process_file(char *file, incident_list_t **list)
{
    if (opts.verbose)
        printf("processing file %s\n", file);

    int fd;
    if ( (fd = open(file, O_RDONLY)) == -1 ) {
        fprintf(stderr, "unable to open file \"%s\": %s\n", file, strerror(errno));
        close(fd);
        return -1;
    }

    /* read header */
    file_header_t header;
    int len;
    if ((len = read(fd, &header, sizeof(header))) == -1) {
        fprintf(stderr, "%s: read error: %s\n", file, strerror(errno));
        close(fd);
        return -1;
    }

    if (len < (signed int)sizeof(header)) {
        fprintf(stderr, "%s: incomplete file header: got %d bytes\n", file, len);
        close(fd);
        return -2;
    }

    if (opts.verbose >= 2) {
        printf("header says:\n");
        printf("    magic: 0x%04x\n", header.magic);
        printf("    version: 0x%04x\n", header.version);
        printf("    flags: 0x%x\n", header.flags);
        printf("    blocks: %d\n", header.NumBlocks);
        printf("    ident: \"%s\"\n", header.ident);
    }

    if (header.magic != FILE_MAGIC) {
        fprintf(stderr, "%s: wrong magic: 0x%04x\n", file, header.magic);
        close(fd);
        return -3;
    }

    if (header.version != FILE_VERSION) {
        fprintf(stderr, "%s: file has newer version %d, this program "
                "only supports version %d\n", file, header.version, FILE_VERSION);
        close(fd);
        return -4;
    }

    if (header.flags != 0) {
        fprintf(stderr, "%s: file is compressed, this is not supported\n", file);
        close(fd);
        return -5;
    }

    /* read stat record */
    stat_record_t stats;
    if ((len = read(fd, &stats, sizeof(stats))) == -1) {
        fprintf(stderr, "%s: read error: %s\n", file, strerror(errno));
        close(fd);
        return -1;
    }

    if (len < (signed int)sizeof(stat_record_t)) {
        fprintf(stderr, "%s: incomplete stat record\n", file);
        close(fd);
        return -6;
    }

    if (opts.verbose >= 2) {
        printf("stat:\n");
        printf("    flows: %llu\n", (unsigned long long)stats.numflows);
        printf("    bytes: %llu\n", (unsigned long long)stats.numbytes);
        printf("    packets: %llu\n", (unsigned long long)stats.numpackets);
        printf("-------------------\n");
    }

    while(header.NumBlocks--) {

        /* read block header */
        data_block_header_t bheader;
        if ( (len = read(fd, &bheader, sizeof(bheader))) == -1) {
            fprintf(stderr, "%s: read error: %s\n", file, strerror(errno));
            close(fd);
            return -1;
        }

        if ( len < (signed int)sizeof(bheader)) {
            fprintf(stderr, "%s: incomplete data block header\n", file);
            close(fd);
            return -7;
        }

        if (opts.verbose >= 3) {
            printf("    data block header:\n");
            printf("        data records: %d\n", bheader.NumBlocks);
            printf("        size: %d bytes\n", bheader.size);
            printf("        id: %d\n", bheader.id);
        }

        if (bheader.id != DATA_BLOCK_TYPE_1) {
            fprintf(stderr, "%s: data block has unknown id %d\n", file, bheader.id);
        }

        /* read complete block into buffer */
        void *buf = malloc(bheader.size);

        if (buf == NULL) {
            fprintf(stderr, "unable to allocate %d byte of memory\n", bheader.size);
            exit(3);
        }

        if ( (len = read(fd, buf, bheader.size)) == -1) {
            fprintf(stderr, "%s: read error: %s\n", file, strerror(errno));
            close(fd);
            return -1;
        }

        if ( len < (signed int)bheader.size ) {
            fprintf(stderr, "%s: incomplete data block\n", file);
            close(fd);
            return -7;
        }

        common_record_t *c = buf;

        while (bheader.NumBlocks--) {
            /* expand common record into master record */
            master_record_t mrec;
            ExpandRecord(c, &mrec);

            /* advance pointer */
            c = (common_record_t *)((pointer_addr_t)c + c->size);

            process_flow(&mrec, list);
        }

        free(buf);
    }

    close(fd);
    return 0;
}

int main(int argc, char *argv[])
{
    const struct option longopts[] = {
        {"help", no_argument, 0, 'h'},
        {"verbose", no_argument, 0, 'v'},
        {"threshhold", required_argument, 0, 't'},
        { NULL, 0, 0, 0 }
    };

    /* initialize options */
    opts.verbose = 0;
    opts.threshhold = 100;

    int c;
    while ((c = getopt_long(argc, argv, "hvt:", longopts, 0)) != -1) {
        switch (c) {
            case 'h': print_help(stdout);
                      exit(0);
                      break;
            case 'v': opts.verbose++;
                      break;
            case 't': opts.threshhold = atoi(optarg);
                      break;
            case '?': print_help(stderr);
                      exit(1);
                      break;
        }
    }

    if (opts.verbose)
        printf("threshhold is %u\n", opts.threshhold);

    if (argv[optind] == NULL)
        printf("no files given, use %s --help for more information\n", argv[0]);

    /* init incident list */
    incident_list_t *list = list_init(INCIDENT_LIST_INITIAL, INCIDENT_LIST_EXPAND);
    list->flows = 0;
    list->incident_flows = 0;

    while (argv[optind] != NULL)
        process_file(argv[optind++], &list);

    if (opts.verbose)
        printf("scanned %u flows, found %u incident flows (%.2f%%)\n", list->flows,
                list->incident_flows, (double)list->incident_flows/(double)list->flows * 100);

    for (unsigned int h = 0; h < HASH_SIZE; h++) {
        if (list->hashtable[h]->fill) {
            // printf("hash %4x:\n", h);
            hashtable_entry_t *ht = list->hashtable[h];
            for (unsigned int i = 0; i < ht->fill; i++) {
                if (ht->records[i]->fill > opts.threshhold) {

                    char src[IPV4_ADDR_STR_LEN_MAX];

                    /* convert source ip to network byte order */
                    ht->records[i]->srcaddr = htonl(ht->records[i]->srcaddr);
                    /* make string from ip */
                    inet_ntop(AF_INET, &ht->records[i]->srcaddr, src, sizeof(src));

                    printf("  * %15s -> %5u : %10u dsthosts (%10u flows)\n",
                            src, ht->records[i]->dstport,
                            ht->records[i]->fill, ht->records[i]->flows);
                }
            }
        }
    }

    list_free(list);

    return EXIT_SUCCESS;
}
