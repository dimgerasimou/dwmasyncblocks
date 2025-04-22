/* See LICENSE file for copyright and license details. */

#define CLICKABLE_BLOCKS 0
#define LEADING_DELIMITER 1
#define DELIMITER "|"
#define TRIM_TRAILING_SPACES 1

#define SCRIPTPATH(SCRIPTNAME) "$HOME/.local/bin/dwmblocks/" #SCRIPTNAME

const Block blocks[] = {
	/*Command                  Update Interval   Update Signal */
	{ SCRIPTPATH("volume"),    0,                10 },
	{ SCRIPTPATH("memory"),    6,                12 },
	{ SCRIPTPATH("keyboard"),  0,                3  },
	{ SCRIPTPATH("kernel"),    360,              4  },
	{ SCRIPTPATH("date"),      300,              6  },
	{ SCRIPTPATH("time"),      1,                5  },
	{ SCRIPTPATH("internet"),  5,                1  },
	{ SCRIPTPATH("battery"),   5,                2  },
	{ SCRIPTPATH("bluetooth"), 5,                15 },
	{ SCRIPTPATH("power"),     0,                14 },
};
