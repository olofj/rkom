
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <paths.h>
#include <errno.h>
#include <fcntl.h>

#include "rkom_proto.h"
#include "exported.h"

#include "rkom.h"
#include "write.h"
#include "set.h"
#include "next.h"

static char *get_text(void);
static char *input_string(void);
static void parse_text(char *);
static void extedit(void);
static char *show_format(void);

static struct rk_misc_info *mi;
static int nmi = 0;
static char *ctext = 0;

#define	TW if (!is_writing) {printf("Du skriver ingen text just nu.\n");return;}

void
write_new(char *str)
{
	char *txt;

	if (is_writing) {
		printf("Du h�ller redan p� att skriva en text.\n");
		return;
	}
	if (myuid == 0) {
		printf("Du m�ste logga in f�rst.\n");
		return;
	}
	if (curconf == 0) {
		printf("Du m�ste g� till ett m�te f�rst.\n");
		return;
	}

	is_writing = 1;
	mi = calloc(sizeof(struct rk_misc_info), 1);
	mi[0].rmi_type = recpt;
	mi[0].rmi_numeric = curconf;
	nmi = 1;
	if (use_editor) {
		extedit();
	} else {
		txt = get_text();
		parse_text(txt);
		free(txt);
	}
}

void
parse_text(char *txt)
{
	char *cmd, *arg;

	if (ctext)
		free(ctext);
	ctext = 0;

	for (;;) {
		if (*txt != '!') {
			ctext = strdup(txt);
			return;
		}
		txt++;
		arg = strsep(&txt, "\n");
		cmd = strsep(&arg, " ");
		if (txt == NULL || arg == NULL)
			return;
		if (strncasecmp(cmd, "Mottagare:", strlen(cmd)) == 0)
			write_rcpt(arg);
		else if (strncasecmp(cmd, "�rende:", strlen(cmd)) == 0) {
			arg[strlen(arg)] = '\n';
			ctext = strdup(arg);
			return;
		} else if (strncasecmp(cmd, "Kommentar", strlen(cmd)) == 0) {
			if (strncasecmp(arg, "till:", 5) == 0)
				cmd = strsep(&arg, " ");
			write_comment(arg);
		} else
			printf("%s f�rstods inte.\n", cmd);
	}
}

void
write_put(char *str)
{
	struct rk_text_info *rti;
	struct rk_text_retval *rtr;

	TW;

	rti = alloca(sizeof(struct rk_text_info));
	rti->rti_misc.rti_misc_len = nmi;
	rti->rti_misc.rti_misc_val = mi;
	rti->rti_text = ctext;
	rtr = rk_create_text(rti);
	if (rtr->rtr_status)
		printf("write_new: %s\n", error(rtr->rtr_status));
	else
		printf("Text %d har skapats.\n", rtr->rtr_textnr);
	free(ctext);
	free(mi);
	free(rtr);
	nmi = 0;
	is_writing = 0;
	ctext = 0;
}

char *
get_text()
{
	char *str, *base;

	base = calloc(10, 1);
	printf("\n�rende: ");
	fflush(stdout);

	for (;;) {
		str = input_string();
		base = realloc(base, strlen(base) + strlen(str) + 1);
		strcat(base, str);
		if (strlen(str) == 0 || str[strlen(str) - 1] != '\n') {
			free(str);
			return base;
		}
		free(str);
	}
}

char *
input_string()
{
        int i = 0, len;
        char *buf = 0;

        do {
                i++;
                buf = realloc(buf, 80 * i);
                len = read(0, &buf[80 * (i - 1)], 80);
        } while (len == 80);
        buf[80 * (i - 1) + len] = 0;

        return buf;
}

void
write_forget(char *str)
{
	if (is_writing)
		printf("Texten du h�ll p� att skriva �r nu bortkastad.\n");
	else
		printf("Du h�ller inte p� att skriva n�gon text.\n");
	is_writing = 0;
	if (ctext)
		free(ctext);
	if (nmi)
		free(mi);
	ctext = 0;
	nmi = 0;
}

void
write_editor(char *str)
{
	TW;
	extedit();
}

void
write_rcpt(char *str)
{
	struct rk_confinfo_retval *cr;
	int conf, i, num;

	TW;
	cr = rk_matchconf(str, MATCHCONF_PERSON|MATCHCONF_CONF);
	num = cr->rcr_ci.rcr_ci_len;
	if (num == 0) {
		printf("Ingen mottagare matchar \"%s\".\n", str);
		free(cr);
		return;
	} else if (num > 1) {
		printf("Mottagarm�tet var flertydigt. Du kan mena:\n");
		for (i = 0; i < num; i++)
			printf("%s\n", cr->rcr_ci.rcr_ci_val[i].rc_name);
		printf("\n");
		free(cr);
		return;
	}
	conf = cr->rcr_ci.rcr_ci_val[0].rc_conf_no;
	free(cr);
	for (i = 0; i < nmi; i++)
		if (mi[i].rmi_numeric == conf && mi[i].rmi_type == recpt)
			return;
	nmi++;
	mi = realloc(mi, sizeof(struct rk_misc_info) * nmi);
	mi[nmi-1].rmi_type = recpt;
	mi[nmi-1].rmi_numeric = conf;
}

void
write_comment(char *str)
{
	int nr, i;

	TW;
	nr = atoi(str);
	if (nr == 0) {
		printf("Det var ett hemskt d�ligt inl�ggsnummer.\n");
		return;
	}
	for (i = 0; i < nmi; i++)
		if (mi[i].rmi_numeric == nr && mi[i].rmi_type == comm_to)
			return;

	nmi++;
	mi = realloc(mi, sizeof(struct rk_misc_info) * nmi);
	mi[nmi-1].rmi_type = comm_to;
	mi[nmi-1].rmi_numeric = nr;
}

void    
write_whole(char *str)  
{
	char *txt;

	TW;

	txt = show_format();
	puts(txt);
	free(txt);
}

char *
show_format()
{
	struct rk_conference *conf;
	char *ret = calloc(10, 1), *r, buf[40];
	int i;

	for (i = 0; i < nmi; i++) {
		switch(mi[i].rmi_type) {
		case recpt:
			conf = rk_confinfo(mi[i].rmi_numeric);
			r = conf->rc_name;
			ret = realloc(ret, strlen(ret) + strlen(r) + 30);
			strcat(ret, "!Mottagare: ");
			strcat(ret, r);
			strcat(ret, "\n");
			free(conf);
			break;
		case comm_to:
			ret = realloc(ret, strlen(ret) + 40);
			sprintf(buf, "!Kommentar till: %d\n", mi[i].rmi_numeric);
			strcat(ret, buf);
			break;
		default:
			printf("Unknown text type %d.\n", mi[i].rmi_type);
			break;
		}
	}
	ret = realloc(ret, strlen(ret) + (ctext?strlen(ctext):0) + 25);
	strcat(ret, "!�rende: ");
	if (ctext)
		strcat(ret, ctext);
	return ret;
}

void
extedit()
{
	struct stat sb;
	extern char **environ;
	char *editor = getenv("EDITOR");
	char *txt, fil[30], *args[4];
	int f;

	if (editor == 0)
		editor = _PATH_VI;

	strcpy(fil, "/tmp/raggkom.XXXXX");
	f = mkstemp(fil);
	if (f == -1) {
		printf("Det gick inte: %s\n", strerror(errno));
		return;
	}
	txt = show_format();
	write(f, txt, strlen(txt));
	close(f);
	free(txt);
	args[0] = editor;
	args[1] = "+2"; /* XXX */
	args[2] = fil;
	args[3] = 0;
	if (fork() == 0) {
		execve(editor, args, environ);
	}
	wait(&f);
	stat(fil, &sb);
	txt = calloc(sb.st_size + 5, 1);
	f = open(fil, O_RDONLY);
	read(f, txt, sb.st_size);
	close(f);
	unlink(fil);
	parse_text(txt);
	free(txt);
}

void
write_cmnt(char *str)
{
	struct rk_text_stat *ts;
	struct rk_misc_info *mf;
	char *txt;
	int text, i, num;

	if (is_writing) {
		printf("Du h�ller redan p� att skriva en text.\n");
		return;
	}
	if (myuid == 0) {
		printf("Du m�ste logga in f�rst.\n");
		return;
	}

	if (str == 0)
		text = lasttext;
	else if ((text = atoi(str)) == 0) {
		printf("'%s' �r ett j�tted�ligt textnummer.\n", str);
		return;
	}
	ts = rk_textstat(text);
	if (ts->rt_retval) {
		free(ts);
		return;
	}

	mf = ts->rt_misc_info.rt_misc_info_val;
	num = ts->rt_misc_info.rt_misc_info_len;
	mi = calloc(sizeof(struct rk_misc_info), num + 1); /* Max size */
	nmi = 0;
	for (i = 0; i < num; i++)
		if (mf[i].rmi_type == recpt) {
			mi[nmi].rmi_type = recpt;
			mi[nmi].rmi_numeric = mf[i].rmi_numeric;
			nmi++;
		}
	mi[nmi].rmi_type = comm_to;
	mi[nmi].rmi_numeric = text;
	nmi++;
	/* XXX subject */
	is_writing = 1;
	if (use_editor) {
		extedit();
	} else {
		txt = get_text();
		parse_text(txt);
		free(txt);
	}
}