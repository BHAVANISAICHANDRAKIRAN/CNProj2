#include <stdio.h>
#include <ctype.h>


void parseURL(char *url, char *hostname, int *port, char *path)
{
	char nurl[1024], nhostname[1024], portstr[16];
	char* tok_str_ptr = NULL;
	int offset = 1;


	strcpy(nurl, url);
	char *ppath = &(nurl[0]);


	if (NULL != strstr(ppath, "http://"))
	{
		ppath = &(nurl[6]);
		offset += 6;
	}


	tok_str_ptr = strtok(ppath, "/");
	sprintf(nhostname, "%s", tok_str_ptr);


	if (NULL != strstr(nhostname, ":"))
	{
		tok_str_ptr = strtok(nhostname, ":");
		sprintf(hostname, "%s", tok_str_ptr);
		tok_str_ptr = strtok(NULL, ":");
		sprintf(portstr, "%s", tok_str_ptr);
		*port = atoi(portstr);
	}
	else {
		sprintf(hostname, "%s", nhostname);
	}


	ppath = &(url[strlen(hostname) + offset]);
	sprintf(path, "%s", ppath);
	if (strcmp(path, "") == 0)
	{
		sprintf(path, "/");
	}
}

void encryptURL(unsigned char *s, char *enc)
{
	char rfc3986[256] = { 0 };
	int i = 0;
	for (; i < 256; i++) {
		rfc3986[i] = isalnum(i) || i == '~' || i == '-' || i == '.' || i == '_'
			? i : 0;
	}

	char *tb = rfc3986;

	for (; *s; s++) {
		if (tb[*s]) sprintf(enc, "%c", tb[*s]);
		else        sprintf(enc, "%%%02X", *s);
		while (*++enc);
	}
}


