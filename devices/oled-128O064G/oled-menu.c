/*
 * Phoenix-RTOS
 *
 * OLED menu
 *
 * Copyright 2018 Phoenix Systems
 * Author: Andrzej Glowinski
 *
 * %LICENSE%
 */

#include <sys/threads.h>
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/msg.h>

#include <jansson.h>

#include "oled-phy.h"
#include "oled-graphic.h"
#include "gpio.h"
#include "fonts/font.h"

#include "oled-functions.h"

#define MAX_MENU_LEN 22
#define MAX_TITLE_LEN 14
#define MENU_PER_SCREEN 7

#define MENU_TIMEOUT (10*1000*1000)
#define MENU_REFRESH (3*1000*1000)
#define MAX_MENU_DEPTH 5

static const uint64_t logo[] = {
	0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
	0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
	0x0000000000000000, 0x01fffffffff07e00, 0x01e000000000ff00, 0x000000000001ff80,
	0x01fffffffff3ffc0, 0x01ff00000003ffc0, 0x000000000007ffc0, 0x01fffffffff7ffe0,
	0x01fffc000007ffc0, 0x000000000003ffc0, 0x01fffffffff3ffc0, 0x01ffffe00001ff80,
	0x000000000000ff00, 0x007ffffffff03c00, 0x003fffff80000000, 0x000fffffc0000000,
	0x0003ffffe0000000, 0x0000fffff8000000, 0x01c07ffffe000000, 0x01e01fffff800000,
	0x01f807ffffc00000, 0x01fe01fffff00000, 0x01ff80fffff00000, 0x01ffc03ffff00000,
	0x01fff00ffff00000, 0x01fffc07fff00000, 0x01ffff01fff00000, 0x01ffff807ff00000,
	0x01ffffe01ff00000, 0x01fffff80ff00000, 0x01fffffe03f00000, 0x01ffffff00f00000,
	0x01efffffc0300000, 0x01e3fffff0000000, 0x01e0fffffc000000, 0x01e07ffffe000000,
	0x01e01fffff800000, 0x00e007ffffe00000, 0x002001fffff00000, 0x000000fffff00000,
	0x0000007ffff00000, 0x0000007ffff00000, 0x00380073fff00000, 0x007e0071fff00000,
	0x00ff80707ff00000, 0x01ffe0701ff00000, 0x01fff07007f00000, 0x01fffc3003f00000,
	0x01fffe1000f00000, 0x01ffff8000f00000, 0x01ffffe000f00000, 0x01fffff000f00000,
	0x01fffffc00f00000, 0x01fffffe00f00000, 0x00ffffff80f00000, 0x00f8ffffe0f00000,
	0x00783ffff0300000, 0x00781ffffc000000, 0x01fc07ffff000000, 0x01ff31ffff800000,
	0x00ffb8ffffc00000, 0x003ffc3fffc00000, 0x018ffc0fffe00000, 0x01c7fc07ffe00000,
	0x01f1fc01fff00000, 0x01fc7c007ff00000, 0x00fe1c001ff00000, 0x003f8c000ff00000,
	0x000fe00e07f00000, 0x0007f80f07f00000, 0x0001fc0fc7f00000, 0x0000fc0ffff00000,
	0x00e07c07fff00000, 0x00f83c01fff00000, 0x01fc3800ffe00000, 0x01ff3c003fe00000,
	0x01ff8c001fc00000, 0x01cfe00003000000, 0x00e3f00000000000, 0x00f1f80000000000,
	0x007c7c0000000000, 0x007f1c0000000000, 0x001fcc0000000000, 0x000ffc0000000000,
	0x00e3fc0000000000, 0x01f8fc0000000000, 0x01fe7c0000000000, 0x00ff180000000000,
	0x00dfc00000000000, 0x01c7f00000000000, 0x01f3f80000000000, 0x01f8fc0000000000,
	0x00fe3c0000000000, 0x063f9c0000000000, 0x079fc00000000000, 0x07c7f00000000000,
	0x07f1fc0000000000, 0x03fcfc0000000000, 0x00fe3c0000000000, 0x007f8c0000000000,
	0x007fe00000000000, 0x0067f00000000000, 0x0071fc0000000000, 0x007c7c0000000000,
	0x003f3c0000000000, 0x001f8c0000000000, 0x000ffc0000000000, 0x0003fc0000000000,
	0x0001fc0000000000, 0x00007c0000000000, 0x0000180000000000, 0x0000000000000000,
	0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
	0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000
};


enum elem_type {submenu, function};


typedef struct _mtElem {
	enum elem_type type;
	int pcursor;
	struct _mtElem *parent;
	struct _mtElem *children;
	int children_size;

	union {
		void (*f)(char *str, int exec);
		char m[MAX_TITLE_LEN];
	};
} mtElem_t;


static struct {
	time_t last_time;
	time_t last_refresh;
	mtElem_t *menu;
	mtElem_t *menu_ptr;
	int cursor;
	volatile sig_atomic_t done;
} m_common;


static void menu_termHandler(int signum)
{
	m_common.done = 1;
}


static void menu_back(char *dst, int exec)
{
	if (exec) {
		m_common.cursor = m_common.menu_ptr->pcursor;
		m_common.menu_ptr = m_common.menu_ptr->parent;
		return;
	}

	strcpy(dst, "Back");
}

static void menu_command(mtElem_t *m)
{
	if (m == NULL)
		return;

	if (m->type == submenu) {
		m_common.menu_ptr = m;
		m_common.cursor = 0;
	}
	else if (m->type == function && m->f != NULL) {
		m->f(NULL, 1);
	}
}


static void menu_getString(mtElem_t *m, char *dst, int size)
{
	if (m == NULL)
		return;

	if (m->type == submenu)
		strncpy(dst, m->m, size - 1);
	else if (m->type == function && m->f != NULL)
		m->f(dst, 0);

	dst[size - 1] = '\0';
}


static int menu_validateStructure(json_t *json, json_t *dict, const char* lang, int depth)
{
	json_t *title, *children, *child;
	size_t index;
	int res, entries = 0;
	int type;

	if (json == NULL || depth > MAX_MENU_DEPTH)
		return -1;

	children = json_object_get(json, "children");
	if (children == NULL || !json_is_array(children))
		type = function;
	else
		type = submenu;

	title = json_object_get(json, "id");
	if (title == NULL || !json_is_string(title))
		return -1;

	if (type == function) {
		if (oledfun_handleFunction(json_string_value(title)) == NULL)
			return -1;
		else
			return 1;
	}

	json_array_foreach(children, index, child) {
		res = menu_validateStructure(child, dict, lang, depth + 1);
		if (res < 0)
			return -1;
		entries += res;
	}

	/* Additional space for back function */
	if (depth > 0)
		++entries;

	return entries + 1;
}


static int menu_validateJson(json_t *json)
{
	json_t *lang, *structure, *dict;
	int res = 0;

	if (json == NULL)
		return -1;

	lang = json_object_get(json, "lang");
	if (lang == NULL || !json_is_string(lang))
		return -1;

	structure = json_object_get(json, "menus");
	if (structure == NULL || !json_is_object(structure))
		return -1;

	dict = json_object_get(json, "locale");
	if (dict == NULL || !json_is_object(dict))
		return -1;

	res = menu_validateStructure(structure, dict, json_string_value(lang), 0);
	if (res < 0)
		return -1;

	return res;
}


static const char* menu_getLocaleString(json_t *id, json_t* dict, const char *lang)
{
	json_t *title;
	const char *id_name = json_string_value(id);

	title = json_object_get(dict, id_name);
	if (title == NULL || !json_is_object(title))
		return id_name;

	title = json_object_get(title, lang);
	if (title == NULL || !json_is_string(title) || json_string_length(title) == 0)
		return id_name;

	return json_string_value(title);
}


static void menu_mtElemInit(json_t *json, json_t* dict, const char* lang, mtElem_t *m, mtElem_t **ptr)
{
	json_t *id, *children, *child;
	size_t index;

	children = json_object_get(json, "children");
	if (children == NULL || !json_is_array(children))
		m->type = function;
	else
		m->type = submenu;

	id = json_object_get(json, "id");

	if (m->type == function) {
		m->f = oledfun_handleFunction(json_string_value(id));
		return;
	}

	strncpy(m->m, menu_getLocaleString(id, dict, lang), MAX_TITLE_LEN - 1);
	m->m[MAX_TITLE_LEN - 1] = '\0';

	m->children_size = json_array_size(children);
	if (m->children_size <= 0) {
		m->children = NULL;
		return;
	}

	if (m->parent != NULL)
		m->children_size += 1;

	m->children = *ptr;
	*ptr += m->children_size;

	json_array_foreach(children, index, child) {
		m->children[index].parent = m;
		m->children[index].pcursor = index;
		menu_mtElemInit(child, dict, lang, &m->children[index], ptr);
	}

	if (m->parent != NULL) {
		m->children[index].parent = m;
		m->children[index].pcursor = index;
		m->children[index].type = function;
		m->children[index].f = menu_back;
	}

	return;
}


static int menu_init(char *conf)
{
	json_t *root, *structure, *dict, *lang;
	json_error_t error;
	mtElem_t *mtptr;
	int entries;

	root = json_load_file(conf, 0, &error);

	if (root == NULL)
		return -1;

	entries = menu_validateJson(root);

	if (entries < 0) {
		json_decref(root);
		return -1;
	}

	structure = json_object_get(root, "menus");
	dict = json_object_get(root, "locale");
	lang = json_object_get(root, "lang");

	m_common.menu = (mtElem_t *) malloc(entries * sizeof(mtElem_t));
	if (m_common.menu == NULL)
		return -1;

	memset(m_common.menu, 0, entries * sizeof(mtElem_t));
	mtptr = m_common.menu + 1;

	menu_mtElemInit(structure, dict, json_string_value(lang), m_common.menu, &mtptr);

	json_decref(root);
	return 0;
}


static void menu_draw(void)
{
	int i, s;
	char str[MAX_MENU_LEN];

	s = snprintf(str, MAX_MENU_LEN, "=== %s ===", m_common.menu_ptr->m);
	oledgraph_drawStringAbs(0, 0, 128, 8, font_5x7, s, str);
	for (i = 0; i < MENU_PER_SCREEN;  ++i) {
		str[0] = i == m_common.cursor % MENU_PER_SCREEN ? '>' : ' ';
		if ((m_common.cursor / MENU_PER_SCREEN) * MENU_PER_SCREEN + i >= m_common.menu_ptr->children_size)
			str[0] = '\0';
		else
			menu_getString(&m_common.menu_ptr->children[(m_common.cursor / MENU_PER_SCREEN) * MENU_PER_SCREEN + i], str + 1, MAX_MENU_LEN - 1);
		oledgraph_drawStringAbs(0, (i + 1) * 8, 128, 8, font_5x7, strlen(str), str);
	}
}


int main(int argc, char **argv)
{
	int s0 = 0, s1 = 0, gpio3port, gpio3dir;
	time_t time;
	int arg;
	char verify = 0;
	char *confFile;
	json_t *root;
	json_error_t error;
	struct sigaction action;

	while ((arg = getopt(argc, argv, "c")) != -1) {
		if (arg == 'c')
			verify = 1;
	}

	if (optind >= argc) {
		fprintf(stderr,"No config file\n");
		exit(EXIT_FAILURE);
	}
	confFile = argv[optind];

	if (verify) {
		root = json_load_file(confFile, 0, &error);
		if (root == NULL)
			return 1;

		return menu_validateJson(root) < 0;
	}

	m_common.last_time = (time_t)-1;
	m_common.cursor = 0;
	m_common.done = 0;

	memset(&action, 0, sizeof(action));
	action.sa_handler = menu_termHandler;
	sigaction(SIGTERM, &action, NULL);

	gpio3port = gpio_openPort(gpio3);
	gpio3dir = gpio_openDir(gpio3);

	gpio_setPin(gpio3port, 17, 0);
	gpio_setDir(gpio3dir, 17, 0);
	gpio_configMux(pctl_mux_lcd_d12, 0, 5);
	gpio_configPad(pctl_pad_lcd_d12, 0, 1, 1, 1, 0, 2, 0, 0);

	gpio_setPin(gpio3port, 20, 0);
	gpio_setDir(gpio3dir, 20, 0);
	gpio_configMux(pctl_mux_lcd_d15, 0, 5);
	gpio_configPad(pctl_pad_lcd_d15, 0, 1, 1, 1, 0, 2, 0, 0);

	oledphy_init();

	/* Rotate by 180 degrees */
	oledphy_sendCmd(0xc8);
	oledphy_sendCmd(0xa1);

	oledgraph_reset(0, 0, 128, 64);
	oledgraph_fillBitmap(0, 0, 128, 64, logo);
	oledgraph_drawBuffer(0, 0, 128, 64, 0);

	/* Turn on the screen */
	oledphy_sendCmd(0xaf);

	usleep(2 * 1000 * 1000);

	if (menu_init(confFile))
		exit(EXIT_FAILURE);

	m_common.menu_ptr = &m_common.menu[0];

	if (oledfun_init(MAX_MENU_LEN - 1))
		exit(EXIT_FAILURE);

	while (!m_common.done) {
		if (!s0 && !gpio_getPin(gpio3port, 17)) {
			if (m_common.cursor < m_common.menu_ptr->children_size - 1) {
				m_common.cursor++;
			}
			else {
				m_common.cursor = 0;
			}
			gettime(&m_common.last_time, NULL);
			m_common.last_refresh = 0;
			s0 = 1;
		}
		else if (s0 && gpio_getPin(gpio3port, 17)) {
			s0 = 0;
		}

		if (!s1 && !gpio_getPin(gpio3port, 20)) {
			menu_command(&m_common.menu_ptr->children[m_common.cursor]);
			gettime(&m_common.last_time, NULL);
			s1 = 1;
			m_common.last_refresh = 0;
		}
		else if (s1 && gpio_getPin(gpio3port, 20)) {
			s1 = 0;
		}

		gettime(&time, NULL);
		if (time - MENU_TIMEOUT > m_common.last_time) {
			m_common.last_time = (time_t) -1;
			m_common.cursor = 0;
			m_common.menu_ptr = &m_common.menu[0];
			m_common.last_refresh = 0;
		}

		if (time - MENU_REFRESH > m_common.last_refresh) {
			menu_draw();
			m_common.last_refresh = time;
		}

		oledfun_update();
		if (oledfun_status() < 0) {
			oledfun_dcsapInit();
		}

		usleep(100000);
	}

	free(m_common.menu);
	return 0;
}
