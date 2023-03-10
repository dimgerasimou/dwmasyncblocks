#ifndef CONFIGURATION
#define CONFIGURATION

#define CLICKABLE_BLOCKS 1
#define LEADING_DELIMITER 1
#define DELIMITER "  "

#define SCRIPTPATH(SCRIPTNAME) "$HOME/.local/bin/dwmblocks/" #SCRIPTNAME

const Block blocks[] = {
	/*Command                  Update Interval   Update Signal */
	{ SCRIPTPATH("volume"),    0,                10 },
	{ SCRIPTPATH("memory"),    6,                0  },
	{ SCRIPTPATH("keyboard"),  0,                3  },
	{ SCRIPTPATH("kernel"),    360,              4  },
	{ SCRIPTPATH("date"),      300,              6  },
	{ SCRIPTPATH("time"),      1,                5  },
	{ SCRIPTPATH("battery"),   5,                2  },
	{ SCRIPTPATH("spacer-0"),  0,                11 },
	{ SCRIPTPATH("internet"),  5,                1  },
	{ SCRIPTPATH("spacer"),    0,                11 }
};

#endif /* ifndef CONFIGURATION */
