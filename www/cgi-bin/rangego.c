#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
//#include <unistd.h>
#include <sys/stat.h>
#include <locale.h>

#define MAX_NAME_LEN 512
#define MAX_FILENAME_LEN 2048
#define MAX_RANKING_LEN 1000
#define MIN_CHARS_TO_LOG 500
#define BACKUP_DIR "scores_backup/"
#define RANKING_DIR "../htdocs/top10/"
#define RANKING_TEMPLATE "../htdocs/top10/topbot.htm"

typedef struct STATISTICS
{
	char lang[2];          // Language code
	char genv;             // Graphical environment: 'x' = X; 'w' = Windows
	time_t when;           // Epoch of stats logging
	int nchars;            // Number of chars typed in the test
	float accur;
	float velo;
	float fluid;
	float score;           // s = f (accur, velo, fluid)
	int name_len;
	char name[MAX_NAME_LEN + 1];
} Statistics;

Statistics *top;
long int top_n = 0;

/* General functions */
void message (char *msg);
/* Statistics functions */
int read_stats_from_file (Statistics *stats, FILE *fh);
void write_stats_to_file (FILE *fh, Statistics *stats);
void insert_stat (Statistics *user_stats, long int i);
void compare_insert_stats (Statistics *user_stats);
char * get_scoring_rows (void);
void print_html_from_stats (FILE *fh, char *langcode);

int
main (int argc, char **argv)
{
	char *tmp;
	char *html = NULL;
	char langcode[3];
	char filename[MAX_FILENAME_LEN + 1];
	char filename_new[MAX_FILENAME_LEN + 1];
	char path[2 * MAX_FILENAME_LEN];
	long int len;
	long int size;
	FILE *fh;
	DIR *dir;
	struct dirent *dent;
	Statistics top10_user[10];

	top = malloc ((MAX_RANKING_LEN + 1) * sizeof (Statistics));
	//memset (top, 0, MAX_RANKING_LEN * sizeof (Statistics)); 

	setlocale (LC_NUMERIC, "pt_BR");

	/* Method test */
	if (getenv ("REQUEST_METHOD") == NULL)
	{
		message ("CGI-eraro: neniu metodo! Ĉu vi lanĉas tiun ĉi CGI-programon komand-linie?");
		return 0;
	}
	if (strcmp (getenv ("REQUEST_METHOD") , "GET") != 0)
	{
		message ("CGI-eraro: maltrafa metodo, ni bezonas GET.");
		return 0;
	}

	/* Language code test */
	if (getenv ("QUERY_STRING") == NULL)
	{
		message ("CGI-eraro: neniu parametro!");
		return 0;
	}
	if (strstr (getenv ("QUERY_STRING"), "lingvo=") == NULL)
	{
		message ("CGI-eraro: neniu parametro 'lingvo'!");
		return 0;
	}
	tmp = strstr (getenv ("QUERY_STRING"), "lingvo=") + 7;
	len = strchr (tmp, '&') == NULL ? strlen (tmp) : strchr (tmp, '&') - tmp;
	if (len > 2)
		len = 2;
	memcpy (langcode, tmp, len);
	langcode[len] = '\0';

	/* Check user-data backup dir at host */
	strcpy (path, BACKUP_DIR);
	if ((dir = opendir (path)))
		closedir (dir);
	else
	{
		message ("Servil-eraro: neniu dosierujo " BACKUP_DIR);
		return 0;
	}

	/* Check ranking dir */
	if ((dir = opendir (RANKING_DIR)))
		closedir (dir);
	else
	{
		message ("Servil-eraro: ne ekzistas la dosierujon " RANKING_DIR);
		return 0;
	}

	/* Read user-data from backup files */
	strcat (path, langcode);
	if ((dir = opendir (path)) == NULL)
	{
		message ("Eraro: ne ekzistanta lingva dosierujo:");
		printf ("<p> %s</p>", path);
		return 0;
	}
	strcat (path, "/");

	while ( (1) )
	{
		if ( !(dent = readdir (dir)) )
			break;
		// Existence tests
		if ( (tmp = strrchr (dent->d_name, '_')) == NULL)
			continue;
		strcpy (filename, langcode);
		strcat (filename, ".ksc");
		if (strcmp (tmp + 1, filename) != 0)
			continue;
		strcpy (filename, path);
		strcat (filename, dent->d_name);
		if ( (fh = fopen (filename, "r")) == NULL)
			continue;

		/* User-data validation */
		if (read_stats_from_file (&top10_user[0], fh) == 0)
		{
			strcpy (filename_new, BACKUP_DIR);
			strcat (filename_new, dent->d_name);
			rename (filename, filename_new);
			fclose (fh);
			continue;
		}
		else
		{
			compare_insert_stats (&top10_user[0]); // Mix user-data with ranking-data, which is global: top
		}

		fclose (fh);
	}
	closedir (dir);

	/* Update html ranking files for site
	 */
	if ( (fh = fopen (RANKING_TEMPLATE, "r")) == NULL)
	{
		message ("Servil-eraro: ne eblas legi html-ŝablonon por poentaro");
		printf ("<h4> %s</h4><br>", RANKING_TEMPLATE);
		return 0;
	}
	print_html_from_stats (fh, langcode);
	fclose (fh);

	return 0;
}

void
message (char *msg)
{
	static int started = 0;

	if (!started)
	{
		started = 1;
		printf ("Content-type: text/html\n\n");
	}
	printf ("<h3>%s</h3><br>\n", msg);
}

int
read_stats_from_file (Statistics *top10, FILE *fh)
{
	int i, n;

	for (i = 0; i < 10; i++)
	{
		/* lang[0] */
		top10[i].lang[0] = fgetc (fh);
		if (!isalpha (top10[i].lang[0]))
			return 0;

		/* lang[1] */
		top10[i].lang[1] = fgetc (fh);
		if (!isalpha (top10[i].lang[1]))
			return 0;

		/* genv */
		top10[i].genv = fgetc (fh);
		if (top10[i].genv != 'x' && top10[i].genv != 'w')
			return 0;

		/* when */
		n = fread (&top10[i].when, 4, 1, fh);
		if (n == 0)
			return 0;

		/* nchars */
		n = fread (&top10[i].nchars, 4, 1, fh);
		if (n == 0 || top10[i].nchars < MIN_CHARS_TO_LOG)
			return 0;

		/* accur */
		n = fread (&top10[i].accur, sizeof (float), 1, fh);
		if (n == 0 || top10[i].accur < 0 || top10[i].accur > 100)
			return 0;

		/* velo */
		n = fread (&top10[i].velo, sizeof (float), 1, fh);
		if (n == 0 || top10[i].velo < 0 || top10[i].velo > 300)
			return 0;

		/* fluid */
		n = fread (&top10[i].fluid, sizeof (float), 1, fh);
		if (n == 0 || top10[i].fluid < 0 || top10[i].fluid > 100)
			return 0;

		/* score */
		n = fread (&top10[i].score, sizeof (float), 1, fh);
		if (n == 0 || top10[i].score < 0 || top10[i].score > 20)
			return 0;

		/* name_len */
		n = fread (&top10[i].name_len, 4, 1, fh);
		if (n == 0 || top10[i].name_len < 0 || top10[i].name_len > MAX_NAME_LEN)
			return 0;

		/* name */
		n = fread (&top10[i].name, sizeof (char), top10[i].name_len, fh);
		top10[i].name[top10[i].name_len] = '\0';
	}

	return 1;
}

void
insert_stat (Statistics *stat, long int i)
{
	long int j;
	static long int stlen = sizeof (Statistics);

	if (stat->score == 0)
		return;
	if (i > top_n)
		return;
	if (i == top_n)
	{
		if (top_n == MAX_RANKING_LEN)
			return;
		memmove (&top[i], stat, sizeof (Statistics));
		top_n++;
		return;
	}

	for (j = top_n - 2; j >= i; j--)
		memmove (&top[j+1], &top[j] , stlen);
	memmove (&top[i], stat, stlen);
	top_n++;
}

void
compare_insert_stats (Statistics *stats)
{
	int h;
	long int a, b, m;

	for (h = 0; h < 10; h++)
	{
		if (stats[h].score == 0)
			return;
		if (top_n == 0)
		{
			insert_stat (&stats[h], 0);
			continue;
		}
		a = 0;
		b = top_n - 1;
		m = (a + b) / 2;
		while (b > a)
		{
			if (stats[h].score > top[m].score)
				b = (b == m ? a: m);
			else if (stats[h].score < top[m].score)
				a = (a == m ? b: m);
			else
			{
				if (stats[h].when < top[m].when)
					m++;
				break;
			}
			m = (a + b) / 2;
		}
		insert_stat (&stats[h], m);
	}
}

char *
get_scoring_rows ()
{
	char *buf;
	char tmp[300];
	long int i;

	buf = malloc (300*top_n);

	if (top_n > 0)
	{
		sprintf (buf,
			"    <tr class=\"unua\">\n"
			"     <td><b>&nbsp;1&nbsp;</b></td>\n"
			"     <td>%s</td>\n"
			"     <td>&nbsp;%1.5f&nbsp;</td>\n"
			"     <td>&nbsp;%s&nbsp;</td>\n"
			"    </tr>\n"
			, top[0].name, top[0].score, top[0].genv == 'x' ? "POSIX" : "MS-Windows");
	}

	if (top_n > 1)
	{
		sprintf (tmp,
			"    <tr class=\"dua\">\n"
			"     <td><b>&nbsp;2&nbsp;</b></td>\n"
			"     <td>%s</td>\n"
			"     <td>&nbsp;%1.5f&nbsp;</td>\n"
			"     <td>&nbsp;%s&nbsp;</td>\n"
			"    </tr>\n"
			, top[1].name, top[1].score, top[1].genv == 'x' ? "POSIX" : "MS-Windows");
		strcat (buf, tmp);
	}

	if (top_n > 2)
	{
		sprintf (tmp,
			"    <tr class=\"tria\">\n"
			"     <td><b>&nbsp;3&nbsp;</b></td>\n"
			"     <td>%s</td>\n"
			"     <td>&nbsp;%1.5f&nbsp;</td>\n"
			"     <td>&nbsp;%s&nbsp;</td>\n"
			"    </tr>"
			, top[2].name, top[2].score, top[2].genv == 'x' ? "POSIX" : "MS-Windows");
		strcat (buf, tmp);
	}

	for (i = 3; i < top_n; i++)
	{
		if (top[i].score == 0)
			continue;
		sprintf (tmp, "\n"
			"    <tr>\n"
			"     <td><b>&nbsp;%i&nbsp;</b></td>\n"
			"     <td>%s</td>\n"
			"     <td>&nbsp;%1.5f&nbsp;</td>\n"
			"     <td>&nbsp;%s&nbsp;</td>\n"
			"    </tr>"
			, i + 1, top[i].name, top[i].score, top[i].genv == 'x' ? "POSIX" : (top[i].genv == 'w' ? "MS-Windows" : "??"));
		strcat (buf, tmp);
	}

	return (buf);
}

#define MEMBLK 4096
#define NTOKS 3+1 /* 3 language substituons plus name and score substitutions */
#define LANGCODE "LINGVO"
#define RANK "RANGO"
void
print_html_from_stats (FILE *fh, char *langcode)
{
	int chr;
	const char *lingvo = LANGCODE;
	const char *ranking = RANK;
	const char *empty = "&nbsp;";
	long int i;
	long int tmp_len;
	char *tmp;

	struct TOKEN
	{
		char *from;
		int from_len;
		char *to;
		int to_len;
	} tok[NTOKS];
	int tki;

	struct STRING_BUFFER
	{
		char *txt;
		long int size;
		long int len;
	} html;
	
	/* Set tokens (from - to)
	 */
	for (i = 0; i < 3; i++)
	{
		tok[i].from = (char *) lingvo;
		tok[i].from_len = strlen (lingvo);
		tok[i].to = langcode;
		tok[i].to_len = 2;
	}
	tok[3].from = (char *) ranking;
	tok[3].from_len = strlen (ranking);

	/* Substitute tokens
	 */
	html.txt = malloc (MEMBLK);
	html.size = MEMBLK;
	html.len = 0;
	i = 0;
	tki = 0;
	while ((chr = getc (fh)) != EOF)
	{
		/* Copy read char */
		if (html.len >= html.size - 1)
		{
			html.size += MEMBLK;
			html.txt = realloc (html.txt, html.size);
		}
		html.txt[html.len] = chr;
		html.len++;

		if (i == NTOKS)
			continue;

		/* one more char equal? */
		if (chr == tok[i].from[tki])
		{
			tki++;
			/* token found! */
			if (tki == tok[i].from_len)
			{
				html.len -= tki;
				if (i < NTOKS - 1)
				{
					if (html.len + tok[i].to_len >= html.size - 1)
					{
						html.size += MEMBLK;
						html.txt = realloc (html.txt, html.size);
					}
					memcpy (html.txt + html.len, tok[i].to, tok[i].to_len);
					html.len += tok[i].to_len;
					tki = 0;
				}
				else
				{
					html.txt[html.len] = '\0';
					tmp = get_scoring_rows ();
					tmp_len = strlen (tmp);
					html.size += tmp_len;
					html.txt = realloc (html.txt, html.size);
					strcat (html.txt, tmp);
					html.len += tmp_len;
					free (tmp);
				}
				i++;
			}
		}
		else
			tki = 0;
	}
	printf ("Content-type: text/html\n\n");
	html.txt[html.len] = '\0';
	printf (html.txt);
}
