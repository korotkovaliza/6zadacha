#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <fcntl.h>

enum Max_length { MAX_LENGTH_NAME = 127,  MAX_LENGTH_ADDRESS = 511, MAX_LENGTH_COMMAND = 1023 };

enum { SEARCH_ONCE = 0, SEARCH_REC = 1 };

struct time
{
	int sec;
	int min;
	int hour;
	int day;
	int month;
	int year;
};

struct file_info
{
	char name[MAX_LENGTH_NAME + 1];
	char address[MAX_LENGTH_ADDRESS + 1];
	int mode;
	char type[MAX_LENGTH_NAME + 1];
	struct time last_change_time;
	time_t last_change_time_in_sec;
};

int search_file(char*, char*, int*, struct file_info*);
int get_info(char*, struct stat*, struct file_info*);
int timecpy(struct time*, struct tm*);
int dircpy(char*, char*);
int check_file(char*, char*, int);
int cpyfile(char*, char*);
int gzip_file(char*);
void print_info(struct file_info*);
void check_last_time();
void remember_last_time();
int get_info_from_address(char*, struct file_info*);

time_t last_time;

int main(int argc, char** argv)
{
	check_last_time();
	char address[MAX_LENGTH_ADDRESS + 1];
	int deep1 = SEARCH_REC;
	int deep2 = SEARCH_REC;
	struct file_info dir_from;
	struct file_info dir_to;
	if (!get_info_from_address(argv[1], &dir_from))
	{
		printf("Error: directory <from> was not found\n");
		exit(-1);
	}
	print_info(&dir_from);
	if (!get_info_from_address(argv[2], &dir_to))
	{
		printf("Error: directory <to> was not found\n");
		exit(-1);
	}
	print_info(&dir_to);
	printf("\nprocess of backup...\n");
	dircpy(dir_to.address, dir_from.address);
	remember_last_time();
	printf("\nBackup completed successfully\n\n");
	return 0;
}

int search_file(char* search_name, char* this_address, int* deep, struct file_info* f_info)
{
	//printf("Address in search: %s\n", this_address);
	DIR* dir;
	int size_old_address = strlen(this_address);
	struct dirent* main_info;
	if ((dir = opendir(this_address)) == NULL)
	{
		printf("Error: can\'t open this directory (in search), addres: <%s>\n", this_address);
		exit(-1);
	}
	struct stat info;
	while ((main_info = readdir(dir)) != NULL)
	{
		if ( (!strcmp(main_info->d_name, ".")) || (!strcmp(main_info->d_name, "..")) )
		{
			continue;
		}
		strcat(this_address, "/");
		strcat(this_address, main_info->d_name);
		if(stat(this_address, &info))
		{
			printf("Error: can\'t make stat structure (for file in address: %s)\n", this_address);
			exit(-1);
		}
		//get_info(main_info->d_name, &info, f_info);
		if (!strcmp(main_info->d_name, search_name))
		{
			get_info(main_info->d_name, &info, f_info);
			strcpy(f_info->address, this_address);
			this_address[size_old_address] = '\0';
			return 1;
		}
		if (S_ISDIR(info.st_mode) && ((*deep) > 0))
		{
			(*deep)++;
			if (search_file(search_name, this_address, deep, f_info))
				return 1;
		}
		this_address[size_old_address] = '\0';
	}
	//printf("deep = %d\n", (*deep) - 1);
	closedir(dir);
	(*deep)--;
	return 0;
}

int get_info(char* name, struct stat* info_ptr, struct file_info* f_info)
{
	struct tm* time = localtime(&(info_ptr->st_mtime));
	f_info->last_change_time_in_sec = info_ptr->st_mtime;
	strcpy(f_info->name, name);
	f_info->mode = ( ((info_ptr->st_mode) & 0x38) >> 3 );
	if (!timecpy(&(f_info->last_change_time), time))
		exit(-1);
	if (S_ISDIR(info_ptr->st_mode))
		strcpy(f_info->type, "It's directory");
	if (S_ISREG(info_ptr->st_mode))
		strcpy(f_info->type, "It's regular file");
	return 1;
}

int timecpy(struct time* where_to, struct tm* from)
{
	if ( (where_to == NULL) || (from == NULL) )
	{
		printf("Error: invalid pointer on struct in functio <timecpy>\n");
		return 0;
	}
	where_to->sec = from->tm_sec;
	where_to->min = from->tm_min;
	where_to->hour = from->tm_hour;
	where_to->day = from->tm_mday;
	where_to->month = from->tm_mon + 1;
	where_to->year = from->tm_year + 1900;
	return 1;
}

int dircpy(char* address_to, char* address_from)
{
	DIR* dir;
	int size_old_address_to = strlen(address_to);
	int size_old_address_from = strlen(address_from);
	struct dirent* main_info;
	if ((dir = opendir(address_from)) == NULL)
	{
		printf("Error: can\'t open this directory (in dircpy)\n");
		exit(-1);
	}
	struct stat info;
	while ((main_info = readdir(dir)) != NULL)
	{
		if ( (!strcmp(main_info->d_name, ".")) || (!strcmp(main_info->d_name, "..")) )
		{
			continue;
		}
		printf("Address from before (in dircpy): %s\n", address_from);
		strcat(address_from, "/");
		strcat(address_to, "/");
		strcat(address_from, main_info->d_name);
		strcat(address_to, main_info->d_name);
		printf("Address from after (in dircpy): %s\n", address_from);
		if(stat(address_from, &info))
		{
			printf("Error: can\'t make stat structure (for file in address: %s)\n", address_from);
			exit(-1);
		}
		if (S_ISDIR(info.st_mode))
		{
			//printf("Address_to before search: %s\n", address_to);
			if (!check_file(main_info->d_name, address_to, size_old_address_to))
				mkdir(address_to, S_IRWXU | S_IRWXG | S_IRWXO);
			//printf("Address_to after search: %s\n", address_to);
			dircpy(address_to, address_from);
		}
		else
		{
			if (!check_file(main_info->d_name, address_to, size_old_address_to))
				cpyfile(address_to, address_from);
		}
		address_from[size_old_address_from] = '\0';
		address_to[size_old_address_to] = '\0';
		//printf("\t%s\n%s\n", address_from, address_to);
	}
	return 0;
}

int check_file(char* name, char* address_to, int size_name)
{
	int deep = SEARCH_ONCE;
	address_to[size_name] = '\0';
	//printf("address to before (in check_file): %s\n", address_to);

	struct file_info f_info;
	int flag_name = 0;
	int flag_name_gz = 0;
	time_t last_change = 0;
	flag_name = search_file(name, address_to, &deep, &f_info);
	last_change = f_info.last_change_time_in_sec;
	char name_gz[MAX_LENGTH_NAME + 1];
	strcpy(name_gz, name);
	strcat(name_gz, ".gz");
	flag_name_gz = search_file(name_gz, address_to, &deep, &f_info);
	if (flag_name_gz)
		last_change = f_info.last_change_time_in_sec;
	strcat(address_to, "/");
	if (flag_name || flag_name_gz)
	{
		if (f_info.mode & 6)
		{
			if (last_change <= last_time)
			{
				//printf("Change time: %ld, last time %ld, no rewriting\n", last_change, last_time);
				strcat(address_to, name);
				return 1;
			}
			else
			{
				//printf("Change time: %ld, last time %ld, rewriting\n", last_change, last_time);
				strcat(address_to, name);
				return 0;
			}
		}
		else
		{
			printf("Error: permission denied in file <%s>, address: <%s>, mode: <%d>\n", name, f_info.address, f_info.mode);
			exit(-1);
		}
	}
	else
	{
		strcat(address_to, name);
		//printf("address to after (in check_file): %s\n", address_to);
		return 0;
	}
}

int cpyfile(char* address_to, char* address_from)
{
	char command[MAX_LENGTH_COMMAND + 1] = "cat ";
	strcat(command, address_from);
	strcat(command, " > ");
	strcat(command, address_to);
	system(command);
	gzip_file(address_to);
	return 1;
}

int gzip_file(char* address)
{
	if (address == NULL)
	{
		printf("Error: NULL-pointer (in gzip_file)\n");
		exit(-1);
	}
	char command[MAX_LENGTH_COMMAND + 1] = "gzip ";
	strcat(command, address);
	system(command);
	return 1;
}

void print_info(struct file_info* f_info)
{
	printf("Information about file %s:\n", f_info->name);
	printf("\taddress:\t%s\n", f_info->address);
	printf("\tmode:\t\t%d\n", f_info->mode);
	printf("\ttype:\t\t%s\n", f_info->type);
	printf("\tlast time:\t\t%02d:%02d:%02d %02d.%02d.%02d\n", f_info->last_change_time.sec, f_info->last_change_time.min, f_info->last_change_time.hour, f_info->last_change_time.day, f_info->last_change_time.month, f_info->last_change_time.year);
}

void check_last_time()
{
	FILE* f = NULL;
	time_t this_time = time(0);
	if ((f = fopen("last_time.txt", "r")) == NULL)
	{
		f = fopen("last_time.txt", "w");
		fprintf(f, "%ld", this_time);
		last_time = this_time;
		fclose(f);
	}
	else
	{
		fscanf(f, "%ld", &last_time);
		fclose(f);
	}
}

void remember_last_time()
{
	FILE* f = fopen("last_time.txt", "w");
	fprintf(f, "%ld", time(0));
	fclose(f);
}

int get_info_from_address(char* address, struct file_info* f_info)
{
	struct stat info;
	if (stat(address, &info))
	{
		return 0;
	}
	else
	{
		get_info(address, &info, f_info);
		strcpy(f_info->address, address);
		return 1;
	}
}
