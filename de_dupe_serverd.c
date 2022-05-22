#include "de_dupe_engin.h"

int log_fd = -1;
FILE	*metadata_fp = NULL;

struct dedupe_footprint	*dedupe_hashtable[HASH_SIZE];

void
dedup_log_msg(const char *msg) {
	write(log_fd, msg, strlen(msg));
}

int
write_dedupe(int argc, char *argv[]) {
	int		opt = 0, src_fd = -1, file_offset = 0, t_count = 0;
	char		in_filename[64], buff[131072], temp_buf[16384], *str;
	char		*abs_path = NULL;
	char		*cksum;
	short		ret = 0;


	strcpy(in_filename, argv[1]);

	src_fd = open(in_filename, O_RDONLY);
	if (src_fd < 0) {
		dedup_log_msg("failed to open source file\n");
		return DEDUPE_OPEN_FAIL;
	}

	abs_path = realpath(in_filename, NULL);

	file_offset = 0;
	ret = 0;
	t_count = 0;
	str = buff;
	while((ret = read(src_fd, temp_buf, sizeof(temp_buf))) > 0) {
		memcpy(str, temp_buf, ret);
		str = str + ret;
		t_count = t_count + ret;

		if (t_count == sizeof(buff)) {
			cksum = calculate_file_md5(buff, t_count);
			insert_metadata_node(buff, abs_path, cksum, file_offset, t_count);
			free(cksum);
			file_offset += t_count;
			str = buff;
			t_count = 0;
		}
	}

	if (t_count) {
		cksum = calculate_file_md5(buff, t_count);
		insert_metadata_node(buff, abs_path, cksum, file_offset, t_count);
		free(cksum);
	}

	close(src_fd);
	return 0;
}

int
restore_dedupe(int argc, char *argv[]) {
	int		opt = 0, src_fd = -1, file_offset = 0, out_fname = 0;
	char		in_filename[64], out_filename[64], buff[131072], *chrptr = NULL;
	char		*abs_path = NULL, *dir_name, o_filename[64] = {'\0'}, dir_path[64] = {'\0'};
	unsigned int	cksum;
	short		ret = 0;

	strcpy(in_filename, argv[1]);
	strcpy(o_filename, argv[2]);
	/*
	 * set default out file name if used has not provided it.
	 */
	abs_path = realpath(in_filename, NULL);
	strcpy(dir_path, abs_path);
	dir_name = dirname(dir_path);
	sprintf(out_filename, "%s/%s",dir_name, o_filename);
	

	ret = restore_dedupe_fromhash(abs_path, out_filename);
}
int
main(int argc, char *argv[]) {
	int			fd, len, client_fd, temp, count, ret1 = 0;
	int			ret = 0, byte_count = 0;
	struct sockaddr_in	saddr;
	struct	sockaddr_in	caddr;
	char 			buffer[BuffSize + 1], *str, ret_val[8]; 
	char			*args[10];
	pid_t			pid, sid;
	struct dedupe_footprint	dedup_object;
	FILE			*local_fp = NULL;

	pid = fork();
	if (pid < 0) {
		printf("fork failed\n");
		return -1;
	}
	
	if (pid > 0) {
		printf("Configuration daemon server created successfully exiting the parent\n");
		return 0;
	}

	umask(0);
	sid = setsid();

	if(sid < 0) {
		return -1;
	}
	chdir("/");
	close(0);
	close(1);
	close(2);

	log_fd = open(LOGFILE, O_RDWR|O_APPEND|O_CREAT, 0666);

	fd = socket(AF_INET,
	 SOCK_STREAM,
	 0);
	if (fd < 0) {
		dedup_log_msg("failed to create socket\n");
		return -1;
	}

	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons(PORTNUMBER);

	if (bind(fd, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
		close(log_fd);
		close(fd);
		dedup_log_msg("failed to bind\n");
		return -1;
	}

	if (listen(fd, MaxConnects) < 0) {
		close(log_fd);
		close(fd);
		dedup_log_msg("listen failed\n");
		return -1;
	}

	dedup_log_msg("\nDe-Dupe configuration daemon server started\n");

	/*
	 * Read the metadata if it's already exists and fill the hash table accordingly.
	 */	
	local_fp = fopen(DEDUP_METADATA_FILE, "r");
	if (local_fp == NULL) {
		goto skip;
	}
	memset(&dedup_object, 0, sizeof(dedup_object));
	
	while ((byte_count = fread(&dedup_object, sizeof(dedup_object), 1, local_fp)) > 0) {
		ret1 = insert_fileobject_inhash(&dedup_object);
		if (ret1 == DEDUPE_NOMEM) {
			dedup_log_msg("failed to build hashtable due to insufficent memory\n");
			return -1;
		}
	}
skip:
	metadata_fp = fopen(DEDUP_METADATA_FILE, "r+");
	if (metadata_fp == NULL) {
		metadata_fp = fopen(DEDUP_METADATA_FILE, "w");
		if (metadata_fp == NULL) {
			dedup_log_msg("failed to open metadata file\n");
			return -1;
		}
	}

	while (1) {
		len = sizeof(caddr);

		client_fd = accept(fd, (struct sockaddr*) &caddr, &len);
		if (client_fd < 0) {
			continue;
		}

		memset(buffer, '\0', sizeof(buffer));
		if (read(client_fd, buffer, sizeof(buffer)) > 0) {
			temp = atoi(buffer);
			if (temp != MAGIC) {
				dedup_log_msg("invalid magic number\n");
				ret = DEDUPE_INVALID;
				goto _out;
			}

			str = buffer;
			while (*str >= '0' && *str <= '9') {
				str++;
			}
			str++;

			temp = atoi(str);

			/*
			 * log the command received from the client.
			 */
			dedup_log_msg(str);
			count = 0;
			args[count] = str;
			while (*str != '\0') {
				if (*str == ' ') {
					*str = '\0';
					count++;
					args[count] = str + 1;
				}
				str++;
			}

			if (temp == 1) {
				ret = write_dedupe(count, args);
			} else if (temp == 0) {
				ret = restore_dedupe(count, args);
			} else {
				dedup_log_msg("invalid usage\n");
				ret = DEDUPE_INVALID;
				goto _out;
			}
		}
	_out:
		sprintf(ret_val, "%d", ret);
		write(client_fd, ret_val, sizeof(ret_val));
		close(client_fd);
	}
	close(fd);
	close(log_fd);
	fclose(metadata_fp);
	return 0;
}
