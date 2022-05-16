#ifndef DE_DUPE_ENGIN_H
#define DE_DUPE_ENGIN_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>


#define MAGIC			8055
#define MaxConnects		50
#define BuffSize		1024
#define PORTNUMBER		9876
#define HASH_SIZE		16384

#define LOGFILE			"/var/log/de_dupe.log"
#define DEDUP_CONFIG_DIR	"/etc/dedup_confi_dir/"
#define DEDUP_METADATA_FILE	"/etc/dedup_confi_dir/dedupe_metadata"
#define DEDUPE_INVALID		-101
#define DEDUPE_OPEN_FAIL	-102
#define DEDUPE_NOMEM		-103

struct dedupe_submeta {
	char		filename[48];
	unsigned long	offset;
};

struct dedupe_footprint {
	char			data_buf[131072];
	int			data_len;
	char			data_cksum[32];
	int			ref_count;
	int			dedupe_offset;
	struct dedupe_submeta	sub_meta[32];
	struct dedupe_footprint	*next;
};

extern struct dedupe_footprint      *dedupe_hashtable[HASH_SIZE];
extern int insert_metadata_node(char *, char*, char *, int, int);
extern unsigned int crc32c(unsigned char *);
extern FILE *metadata_fp;
extern int insert_fileobject_inhash(struct dedupe_footprint *);
extern int restore_dedupe_fromhash(char *, char *);
extern void dedup_log_msg(const char *);
extern char *calculate_file_md5(const char *, int);

#endif /* DE_DUPE_ENGIN_H */
