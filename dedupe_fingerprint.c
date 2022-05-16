#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/md5.h>
#include <math.h>
#include "de_dupe_engin.h"

int hextodc(char *hex){
	int y = 0;
	int dec = 0;
	int x, i;
	for(i = strlen(hex) - 1 ; i >= 0 ; --i){
		if(hex[i]>='0'&&hex[i]<='9'){
			x = hex[i] - '0';
		}
		else{
			x = hex[i] - 'A' + 10;
		}
		dec = dec + x * pow(16,y);
	}
	return dec;
}

char
*calculate_file_md5(const char *data, int datalen) {
	unsigned char c[MD5_DIGEST_LENGTH];
	int i;
	
	MD5_CTX mdContext;	
	char *filemd5 = (char*)malloc(33*sizeof(char));		
	MD5_Init(&mdContext);	
	
	MD5_Update(&mdContext, data, datalen);
	MD5_Final(c, &mdContext);
	for(i=0; i<MD5_DIGEST_LENGTH; i++) {
		sprintf(&filemd5[i*2], "%02x", (unsigned int)c[i]);
	}	
	return filemd5;
}

unsigned reverse(unsigned x) {
	x = ((x & 0x55555555) <<  1) | ((x >>  1) & 0x55555555);
	x = ((x & 0x33333333) <<  2) | ((x >>  2) & 0x33333333);
	x = ((x & 0x0F0F0F0F) <<  4) | ((x >>  4) & 0x0F0F0F0F);
	x = (x << 24) | ((x & 0xFF00) << 8) |
		((x >> 8) & 0xFF00) | (x >> 24);
	return x;
}

unsigned int
crc32c(unsigned char *message) {
	int i, j;
	unsigned int byte, crc, mask;
	static unsigned int table[256];

	if (message == NULL) {
		return -1;
	}

	/* Set up the table, if necessary. */

	if (table[1] == 0) {
		for (byte = 0; byte <= 255; byte++) {
			crc = byte;
			for (j = 7; j >= 0; j--) {    // Do eight times.
				mask = -(crc & 1);
				crc = (crc >> 1) ^ (0xEDB88320 & mask);
			}
			table[byte] = crc;
		}
	}

	/* Through with table setup, now calculate the CRC. */

	i = 0;
	crc = 0xFFFFFFFF;
	while ((byte = message[i]) != 0) {
		crc = (crc >> 8) ^ table[(crc ^ byte) & 0xFF];
		i = i + 1;
	}
	return ~crc;
}

int
hashcode(char cksum[]) {
	int dec = hextodc(cksum);
	return dec % HASH_SIZE;
}

struct dedupe_footprint *
search_dedupe_node(char cksum[]) {
	struct dedupe_footprint	*tmp =NULL;
	int			hashIndex = hashcode(cksum);

	if (dedupe_hashtable[hashIndex] !=  NULL) {
		tmp = dedupe_hashtable[hashIndex];
		while(tmp != NULL) {
			if (strncmp(cksum, tmp->data_cksum, 32) == 0) {
				return tmp;
			}
			tmp = tmp->next;
		}
	}
	return NULL;
}

int
restore_dedupe_fromhash(char *in_file, char *out_file) {
	struct dedupe_footprint *tmp = NULL, *hold = NULL;
	int			i = 0, refcount = 0, j = 0, loffset = 0;
	int			first_flag = 0, sub_index = 0, hold_offset = 0;

	FILE *fp = fopen(out_file, "w");
	if (fp == NULL) {
		return DEDUPE_OPEN_FAIL;
	}

	for(i = 0; i < HASH_SIZE; i++) {
		tmp = dedupe_hashtable[i];
		while (tmp != NULL) {
			refcount = tmp->ref_count;
			for (j = 0; j < refcount; j++) {

				loffset = tmp->sub_meta[j].offset;

				if (strcmp(tmp->sub_meta[j].filename, in_file) == 0) {

					if (first_flag == 0 &&  loffset != 0) {
						sub_index = j;
						first_flag = 1;
						hold = tmp;
						hold_offset = loffset;
					}

					fseek(fp, loffset, SEEK_SET);
					fwrite(tmp->data_buf, tmp->data_len, 1, fp);
				}
			}
			tmp = tmp->next;
		}
	}
	if (hold) {
		if (strcmp(hold->sub_meta[sub_index].filename, in_file) == 0) {
			fseek(fp, hold_offset, SEEK_SET);
			fwrite(hold->data_buf, hold->data_len, 1, fp);
		}
	}
	return 0;
}

int
insert_fileobject_inhash(struct dedupe_footprint *obj) {
	struct dedupe_footprint		*node = NULL, *tmp = NULL;
	int				hashIndex;

	if (obj == NULL) {
		return -1;
	}

	node = (struct dedupe_footprint *) malloc (sizeof (struct dedupe_footprint));
	if (node == NULL) {
		return DEDUPE_NOMEM;
	}

	strncpy(node->data_buf, obj->data_buf, obj->data_len);
	node->data_len = obj->data_len;
	strncpy(node->data_cksum, obj->data_cksum, 32);
	node->ref_count = obj->ref_count;
	node->dedupe_offset = obj->dedupe_offset;
	memcpy(node->sub_meta, obj->sub_meta, sizeof(obj->sub_meta));

	hashIndex = hashcode(obj->data_cksum);
	if (dedupe_hashtable[hashIndex] !=  NULL) {
		tmp = dedupe_hashtable[hashIndex];
		while(tmp->next != NULL) {
			tmp=tmp->next;
		}
		tmp=tmp->next = node;
	} else {
		dedupe_hashtable[hashIndex] = node;
	}

	return 0;
}

int
insert_metadata_node(char *buff, char* filename, char *cksum, int file_offset, int data_len) {
	struct dedupe_footprint		*node = NULL, *search_node = NULL, *tmp = NULL;
	int				hashIndex = 0, refcnt = 0, dedup_offset = 0;

	search_node = search_dedupe_node(cksum);
	if (search_node == NULL) {
		node = (struct dedupe_footprint *) malloc (sizeof (struct dedupe_footprint));
		if (node == NULL) {
			return DEDUPE_NOMEM;
		}
		strncpy(node->sub_meta[0].filename, filename, 48);
		node->sub_meta[0].offset = file_offset;
		node->ref_count = 1;
		strncpy(node->data_cksum, cksum, 32);
		strncpy(node->data_buf, buff, data_len);
		node->data_len = data_len;

		hashIndex = hashcode(cksum);
		if (dedupe_hashtable[hashIndex] !=  NULL) {
			tmp = dedupe_hashtable[hashIndex];
			while(tmp->next != NULL) {
				tmp=tmp->next;
			}
			tmp=tmp->next = node;
		} else {
			dedupe_hashtable[hashIndex] = node;
		}
		fseek(metadata_fp, 0, SEEK_END);
		node->dedupe_offset = ftell(metadata_fp);
		fwrite(node, sizeof (struct dedupe_footprint), 1, metadata_fp);
	} else {
		refcnt = search_node->ref_count;
		search_node->sub_meta[refcnt].offset = file_offset;
		strncpy(search_node->sub_meta[refcnt].filename, filename, 48);
		search_node->ref_count++;
		fseek(metadata_fp, search_node->dedupe_offset, SEEK_SET);
		fwrite(search_node, sizeof (struct dedupe_footprint), 1, metadata_fp);
	}

	return 0;
}
