/* See LICENSE file for copyright and license details. */

#define CLICKABLE_BLOCKS 0
#define LEADING_DELIMITER 1
#define DELIMITER "|"
#define TRIM_TRAILING_SPACES 1

#define BLOCK(NAME) "$HOME/.local/bin/statusblocks/" #NAME

const Block blocks[] = {
	/*Command             Update Interval   Update Signal */
	{ BLOCK("volume"),    0,                10 },
	{ BLOCK("memory"),    6,                12 },
	{ BLOCK("keyboard"),  0,                3  },
	{ BLOCK("system"),    360,              4  },
	{ BLOCK("date"),      300,              6  },
	{ BLOCK("time"),      1,                5  },
	{ BLOCK("internet"),  5,                1  },
	{ BLOCK("battery"),   5,                2  },
	{ BLOCK("bluetooth"), 5,                15 },
	{ BLOCK("power"),     0,                14 },
};
