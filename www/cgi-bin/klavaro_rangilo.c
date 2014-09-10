#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <locale.h>

#define MAX_NAME_LEN 512
#define MAX_FILENAME_LEN 2048
#define MIN_CHARS_TO_LOG 500
#define BACKUP_DIR "scores_backup/"
#define RANKING_DIR "../htdocs/top10/"
#define RANKING_TEMPLATE "../htdocs/top10/top10.htm"

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

/* General functions */
void message (char *msg);
/* Statistics functions */
int read_stats_from_file (Statistics *stats, FILE *fh);
void write_stats_to_file (FILE *fh, Statistics *stats);
void init_stats (Statistics *stats);
void compare_insert_stats (Statistics *top10, Statistics *user_stats);
char * get_html_from_stats (FILE *fh, Statistics *stats, char *langcode);

int
main (int argc, char **argv)
{
	char *tmp;
	char *html = NULL;
	char langcode[3];
	char filename[MAX_FILENAME_LEN + 1];
	char path[2 * MAX_FILENAME_LEN];
	int len;
	long int size;
	FILE *fh;
	DIR *dir;
	Statistics top10_user[10];
	Statistics top10[10];

	setlocale (LC_NUMERIC, "pt_BR");

	/* Method test */
	if (getenv ("REQUEST_METHOD") == NULL)
	{
		message ("CGI-eraro: neniu metodo! Ĉu vi lanĉas tiun ĉi CGI-programon komand-linie?");
		return 0;
	}
	if (strcmp (getenv ("REQUEST_METHOD") , "PUT") != 0)
	{
		message ("CGI-eraro: maltrafa metodo, ni bezonas PUT.");
		printf ("<h3>Metodo: %s</h3><br>\n", getenv ("REQUEST_METHOD"));
		return 0;
	}

	/* User-data length test */
	if (getenv ("CONTENT_LENGTH") == NULL)
	{
		message ("Enig-eraro: neniu datumaro ricevita, longeco estas nula.");
		return 0;
	}
	size = strtol (getenv ("CONTENT_LENGTH"), NULL, 10);
	if (size < 300 || size > 10*sizeof(Statistics))
	{
		message ("Enig-eraro: malvalida longeco de datumaro.");
		return 0;
	}

	/* File name test */
	if (getenv ("QUERY_STRING") == NULL)
	{
		message ("CGI-eraro: neniu parametro!");
		return 0;
	}
	if (strstr (getenv ("QUERY_STRING"), "dosiernomo=") == NULL)
	{
		message ("CGI-eraro: neniu parametro 'dosiernomo'!");
		return 0;
	}
	tmp = strstr (getenv ("QUERY_STRING"), "dosiernomo=") + 11;
	len = strchr (tmp, '&') == NULL ? strlen (tmp) : strchr (tmp, '&') - tmp;
	if (len > MAX_FILENAME_LEN)
		len = MAX_FILENAME_LEN;
	memcpy (filename, tmp, len);
	filename[len] = '\0';

	/* Language code test */
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
		if (mkdir (path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
		{
			message ("Servil-eraro: ne eblas krei la dosierujon " BACKUP_DIR);
			return 0;
		}
	}
	strcat (path, langcode);
	if ((dir = opendir (path)))
		closedir (dir);
	else
		if (mkdir (path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
		{
			message ("Servil-eraro: ne eblas krei la dosierujon:");
			printf ("<p> %s</p>", path);
			return 0;
		}

	/* User-data validation */
	if (read_stats_from_file (&top10_user[0], stdin))
		return 0; /* the function has already messaged the error... */

	/* Write user-data to backup file */
	strcat (path, "/");
	strcat (path, filename);
	fh = fopen (path, "w");
	if (fh == NULL)
	{
		message ("Servil-eraro: ne eblas krei sekurkopion por dosiero '.ksc'.");
		printf ("<p> %s</p>", path);
	}
	else
	{
		write_stats_to_file (fh, top10_user);
		fclose (fh);
	}

	/* Check ranking dir */
	strcpy (path, RANKING_DIR);
	if ((dir = opendir (path)))
		closedir (dir);
	else
		if (mkdir (path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
		{
			message ("Servil-eraro: ne eblas krei la dosierujon " RANKING_DIR);
			return 0;
		}

	/* Read ranking-data */
	len = strlen (path);
	strcpy (path + len, "global_"); len += 7;
	strcpy (path + len , langcode); len += strlen (langcode);
	strcpy (path + len, ".ksc");
	fh = fopen (path, "r");
	if (fh == NULL)
		init_stats (top10);
	else
	{
		read_stats_from_file (top10, fh);
		fclose (fh);
	}

	/* Mix user-data with ranking-data
	 */
	compare_insert_stats (top10, top10_user);

	/* Write ranking-data
	 */
	fh = fopen (path, "w");
	if (fh == NULL)
	{
		message ("Servil-eraro: ne eblas konservi ĝeneralan poentaron:");
		printf ("<h4> %s</h4><br>", path);
		return 0;
	}
	write_stats_to_file (fh, top10);
	fclose (fh);

	/* Return ranking-data to user
	printf ("Content-type: application/octet-stream\n\n");
	write_stats_to_file (stdout, top10);
	 */

	/* Update html ranking files for site
	 */
	fh = fopen (RANKING_TEMPLATE, "r");
	if (fh == NULL)
	{
		message ("Servil-eraro: ne eblas legi html-ŝablonon por poentaro");
		printf ("<h4> %s</h4><br>", RANKING_TEMPLATE);
		return 0;
	}
	html = get_html_from_stats (fh, top10, langcode);
	fclose (fh);
	if (html == NULL)
	{
		message ("HTML-eraro: ne eblis krei html-ĉenon kun poentaro");
		return 0;
	}
	strcpy (path, RANKING_DIR);
	strcpy (path + strlen(path), langcode);
	if ((dir = opendir (path)))
		closedir (dir);
	else
		if (mkdir (path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))
		{
			message ("Servil-eraro: ne eblas krei la dosierujon:\n");
			printf (" %s", path);
			return 0;
		}
	strcpy (path + strlen(path), "/index.html");

	fh = fopen (path, "w");
	if (fh == NULL)
	{
		message ("Servil-eraro: ne eblas konservi html-poentaron");
		return 0;
	}
	fwrite (html, sizeof (char), strlen (html), fh);
	fclose (fh);

	message ("Farite. Dankon!");
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
	int i, j, n;

	for (i = 0; i < 10; i++)
	{
		/* lang[0] */
		top10[i].lang[0] = fgetc (fh);
		if (!isalpha (top10[i].lang[0]))
		{
			message ("Analizo: lang[0] ne estas litero.");
			return 0;
		}

		/* lang[1] */
		top10[i].lang[1] = fgetc (fh);
		if (!isalpha (top10[i].lang[1]))
		{
			message ("Analizo: lang[1] ne estas litero.");
			return 0;
		}

		/* genv */
		top10[i].genv = fgetc (fh);
		if (top10[i].genv != 'x' && top10[i].genv != 'w')
		{
			message ("Analizo: 'genv' ne estas ikso nek vavo");
			return 0;
		}

		/* when */
		n = fread (&top10[i].when, 4, 1, fh);
		if (n == 0)
		{
			message ("Analizo: 'when' ne validas");
			return 0;
		}

		/* nchars */
		n = fread (&top10[i].nchars, 4, 1, fh);
		if (n == 0 || top10[i].nchars < MIN_CHARS_TO_LOG)
		{
			message ("Analizo: 'nchars' ne validas");
			return 0;
		}

		/* accur */
		n = fread (&top10[i].accur, sizeof (float), 1, fh);
		if (n == 0 || top10[i].accur < 0 || top10[i].accur > 100)
		{
			message ("Analizo: 'accur' ne validas");
			return 0;
		}

		/* velo */
		n = fread (&top10[i].velo, sizeof (float), 1, fh);
		if (n == 0 || top10[i].velo < 0 || top10[i].velo > 300)
		{
			message ("Analizo: 'velo' ne validas");
			return 0;
		}

		/* fluid */
		n = fread (&top10[i].fluid, sizeof (float), 1, fh);
		if (n == 0 || top10[i].fluid < 0 || top10[i].fluid > 100)
		{
			message ("Analizo: 'fluid' ne validas");
			return 0;
		}

		/* score */
		n = fread (&top10[i].score, sizeof (float), 1, fh);
		if (n == 0 || top10[i].score < 0 || top10[i].score > 20)
		{
			message ("Analizo: 'score' ne validas");
			return 0;
		}

		/* name_len */
		n = fread (&top10[i].name_len, 4, 1, fh);
		if (n == 0 || top10[i].name_len < 0 || top10[i].name_len > MAX_NAME_LEN)
		{
			message ("Analizo: 'name_len' ne validas");
			printf ("%i", top10[i].name_len);
			return 0;
		}

		/* name */
		n = fread (&top10[i].name, sizeof (char), top10[i].name_len, fh);
		top10[i].name[top10[i].name_len] = '\0';
	}

	return 0;
}

void
write_stats_to_file (FILE *fh, Statistics *top10)
{
	int i;

	for (i = 0; i < 10; i++)
	{
		fputc (top10[i].lang[0], fh);
		fputc (top10[i].lang[1], fh);
		fputc (top10[i].genv, fh);
		fwrite (&top10[i].when, 4, 1, fh);
		fwrite (&top10[i].nchars, 4, 1, fh);
		fwrite (&top10[i].accur, sizeof (float), 1, fh);
		fwrite (&top10[i].velo, sizeof (float), 1, fh);
		fwrite (&top10[i].fluid, sizeof (float), 1, fh);
		fwrite (&top10[i].score, sizeof (float), 1, fh);
		fwrite (&top10[i].name_len, 4, 1, fh);
		if (top10[i].name && top10[i].name_len > 0)
			fputs (top10[i].name, fh);
	}
}

#define NOBODY "^_^"
void
clean_stat (Statistics *top10, int i)
{

	if (i > 9)
		return;

	top10[i].lang[0] = 'x';
	top10[i].lang[1] = 'x';
	top10[i].genv = 'x';
	top10[i].when = 0;
	top10[i].nchars = MIN_CHARS_TO_LOG;
	top10[i].accur = 0.0;
	top10[i].velo = 0.0;
	top10[i].fluid = 0.0;
	top10[i].score = 0.0;
	top10[i].name_len = strlen (NOBODY);
	strcpy (top10[i].name, NOBODY);
}

void
init_stats (Statistics *top10)
{
	int i;

	for (i = 0; i < 10; i++)
		clean_stat (top10, i);
}

void
insert_stat (Statistics *top10, Statistics *stat, int i)
{
	int j;

	if (i > 9)
		return;

	for (j = 8; j >= i; j--)
		memmove (&top10[j + 1], &top10[j], sizeof (Statistics));

	memmove (&top10[i], stat, sizeof (Statistics));
}

void
delete_stat (Statistics *top10, int i)
{
	int j;

	if (i > 9)
		return;

	for (j = i; j < 9; j++)
		memmove (&top10[j], &top10[j + 1], sizeof (Statistics));

	clean_stat (top10, 9);
}

void
compare_insert_stats (Statistics *top10, Statistics *stats)
{
	int h, i, j, k;
	int statnamelen;
	char *pos;
	time_t tmp_horo;
	struct tm *hodiaux;
	struct tm *kiam;
	long int pasis;
	int jaro, monato, tago;

	message ("----- Komparado -----");
	for (h = 0; h < 10; h++)
	{
		statnamelen = strlen (stats[h].name); 
		pos = strrchr (stats[h].name, '[');
		if (pos != NULL)
			if (pos > stats[h].name && *(pos - 1) == ' ')
			{
				statnamelen = pos - stats[h].name - 1; 
				if (statnamelen < 1)
					statnamelen = strlen (stats[h].name); 
			}
		printf ("<p>%s (Longeco: %i)</p>", stats[h].name, statnamelen);
		for (i = 0; i < 10; i++)
		{
			if (stats[h].score > top10[i].score)
			{
				for (j = i - 1; j >= 0; j--)
				{
					if (strncmp (stats[h].name, top10[j].name, statnamelen) == 0)
						break;
				}
				if (j > -1)
				{
					printf ("<p>Ripetitulo...</p><br>");
					break;
				}

				for (k = i; k < 10; k++)
				{
					if (strncmp (stats[h].name, top10[k].name, statnamelen) == 0 &&
							stats[h].score > 0)
					{
						printf ("<p>Ni forigis ripetitan: %s</p><br>", top10[k].name);
						delete_stat (top10, k);
						k--;
					}
				}
				insert_stat (top10, &stats[h], i);
				printf ("<p>Vi eniris, gratulon!!!</p><br>");
				break;
			}
			else if (i == 9 && stats[h].score > 0)
				printf ("<p>Nenio por vi...</p><br>");
		}
	}
	printf ("<hr>");

	message ("----- Kontrolo pri malnovuloj-----");
	tmp_horo = time (NULL);
	kiam = localtime (&tmp_horo);
	jaro = kiam->tm_year;
	monato = kiam->tm_mon;
	tago = kiam->tm_mday;
	for (i = 0; i < 10; i++)
	{
		if (top10[i].score == 0)
			continue;

		kiam = localtime (&top10[i].when);
		pasis = 365 * (jaro - kiam->tm_year) + 30 * (monato - kiam->tm_mon) + (tago - kiam->tm_mday);
		if (pasis > 365)
		{
			printf ("<p>Ni forigis malnovulon: %s (<b>%li</b> tagoj)</p><br>", top10[i].name, pasis);
			delete_stat (top10, i);
			i--;
		}
		else
		{
			printf ("<p>%s: <b>%li</b> tagoj el 365</p>", top10[i].name, pasis);
			printf ("<p>Kiam: %i-%i-%i</p><br>", kiam->tm_year + 1900, kiam->tm_mon + 1, kiam->tm_mday);
		}
	}
	printf ("<h4>Hodiaux: %i-%i-%i</h4>", jaro + 1900, monato + 1, tago);
	printf ("<hr>");
}

#define MEMBLK 4096
#define NTOKS 3+2*10 /* 3 language substituons plus 10 times name and score substitutions */
#define LANGCODE "LINGVO"
#define NAME "_NOMO_"
#define NAME_LEN 6
#define SCORE "_POENTO_"
#define SCORE_LEN 8
#define NOBODY "^_^"
char *
get_html_from_stats (FILE *fh, Statistics *top10, char *langcode)
{
	int chr;
	const char *lingvo = LANGCODE;
	const char *empty = "&nbsp;";
	char name[10][NAME_LEN+5];
	char score[10][SCORE_LEN+5];
	char top10_score[10][20];
	int i, j, k;

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
		int size;
		int len;
	} html;
	
	/* Set name, score and top10_score
	 */
	for (i = 0; i < 10; i++)
	{
		sprintf (name[i], NAME "%0.2i", i+1);
		sprintf (score[i], SCORE "%0.2i", i+1);
		sprintf (top10_score[i], "%f", top10[i].score);
	}
	/* Set tok[0:2]
	 */
	for (i = 0; i < 3; i++)
	{
		tok[i].from = (char *) lingvo;
		tok[i].from_len = strlen (lingvo);
		tok[i].to = langcode;
		tok[i].to_len = 2;
	}
	/* Set tok[3:]
	 */
	for (j = 0; i < NTOKS && j < 10; j++)
	{
		tok[i].from = name[j];
		tok[i].from_len = NAME_LEN+2;
		if (strcmp (top10[j].name, NOBODY) == 0)
		{
			tok[i].to = (char *) empty;
			tok[i].to_len = strlen (empty);
		}
		else
		{
			tok[i].to = top10[j].name;
			tok[i].to_len = top10[j].name_len;
		}
		i++;

		tok[i].from = score[j];
		tok[i].from_len = SCORE_LEN+2;
		if (strcmp (top10[j].name, NOBODY) == 0)
		{
			tok[i].to = (char *) empty;
			tok[i].to_len = strlen (empty);
		}
		else
		{
			tok[i].to = top10_score[j];
			tok[i].to_len = strlen (top10_score[j]);
		}
		i++;
	}

	/* Substitute tokens
	 */
	html.txt = malloc (MEMBLK);
	html.size = MEMBLK;
	html.len = 0;
	for (i = 0; i < NTOKS; i++)
	{
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
			/* one more char equal? */
			if (chr == tok[i].from[tki])
			{
				tki++;
				/* token found! */
				if (tki == tok[i].from_len)
				{
					html.len -= tki;
					if (html.len + tok[i].to_len >= html.size - 1)
					{
						html.size += MEMBLK;
						html.txt = realloc (html.txt, html.size);
					}
					memcpy (html.txt + html.len, tok[i].to, tok[i].to_len);
					html.len += tok[i].to_len;
					/* goto search next token... */
					if (i < NTOKS -1)
					       	break;
					else
						tki = 0;
				}
			}
			else
				tki = 0;
		}
		if (chr == EOF)
			break;
	}
	html.txt[html.len] = '\0';
	if (i != NTOKS - 1)
	{
		free (html.txt);
		html.txt = NULL;
	}
	return (html.txt);
}
