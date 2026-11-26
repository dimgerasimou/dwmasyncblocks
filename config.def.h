/* See LICENSE file for copyright and license details. */

#define CLICKABLE_BLOCKS 0
#define LEADING_DELIMITER 1
#define DELIMITER "|"
#define TRIM_TRAILING_SPACES 1

/* Colour escape; CLR_FMT gets one "#RRGGBB". Defaults target status2d. */
#define CLR_FMT "^c%s^"
#define CLR_NRM "^d^"

/* Resources are read as XRES_PREFIX "clr_<class>" and XRES_PREFIX "icon_<block>". */
#define XRES_PREFIX "statusblocks."

#define BLOCK(NAME) "$HOME/.local/bin/statusblocks/" #NAME

static const Colour colours[] = {
	/*Class      Default */
	{ "bat_crt", "#F38BA8" },
	{ "bat_low", "#F9E2AF" },
	{ "bat_nrm", "#CDD6F4" },
	{ "bat_chg", "#F9E2AF" },
	{ "bt",      "#CDD6F4" },
	{ "date",    "#CDD6F4" },
	{ "net_nrm", "#CDD6F4" },
	{ "net_err", "#F38BA8" },
	{ "sys_pkg", "#89B4FA" },
	{ "sys_nrm", "#CDD6F4" },
	{ "kbd",     "#CDD6F4" },
	{ "mem",     "#CDD6F4" },
	{ "pwr",     "#F38BA8" },
	{ "tim",     "#CDD6F4" },
	{ "vol_nrm", "#CDD6F4" },
	{ "vol_mut", "#F38BA8" },
	{ "cal",     "#F38BA8" },
};

const Block blocks[] = {
	/*Command             Update Interval   Update Signal   Icons */
	{ BLOCK("volume"),    0,                10,             1 },
	{ BLOCK("memory"),    6,                12,             1 },
	{ BLOCK("keyboard"),  0,                3,              1 },
	{ BLOCK("updates"),   360,              4,              1 },
	{ BLOCK("kernel"),    0,                7,              1 },
	{ BLOCK("date"),      300,              6,              1 },
	{ BLOCK("time"),      1,                5,              1 },
	{ BLOCK("internet"),  5,                1,              1 },
	{ BLOCK("battery"),   5,                2,              1 },
	{ BLOCK("bluetooth"), 5,                15,             1 },
	{ BLOCK("power"),     0,                14,             1 },
};
