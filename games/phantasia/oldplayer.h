/*
 * oldplayer.h - old player structure
 */

struct	oldplayer	    	/* player statistics */
    {
    char	o_name[21];	/* name */
    char	o_password[9];	/* password */
    char	o_login[10];	/* login */
    double	o_x;	    	/* x coord */
    double	o_y;	    	/* y coord */
    double	o_experience;	/* experience */
    int		o_level;    	/* level */
    short	o_quickness;	/* quickness */
    double	o_strength;	/* strength */
    double	o_sin;		/* sin */
    double	o_mana;		/* mana */
    double	o_gold;		/* gold */
    double	o_energy;	/* energy */
    double	o_maxenergy;	/* maximum energy */
    double	o_magiclvl;	/* magic level */
    double	o_brains;	/* brains */
    short	o_crowns;	/* crowns */
    struct
	{
	short	ring_type;	/* type of ring */
	short	ring_duration;	/* duration of ring */
	}	o_ring;	    	/* ring stuff */
    bool	o_palantir;	/* palantir */
    double	o_poison;	/* poison */
    short	o_holywater;   	/* holy water */
    short	o_amulets;	/* amulets */
    bool	o_blessing;	/* blessing */
    short	o_charms;	/* charms */
    double	o_gems;		/* gems */
    short	o_quksilver;	/* quicksilver */
    double	o_sword;	/* sword */
    double	o_shield;	/* shield */
    short	o_type;		/* character type */
    bool	o_virgin;	/* virgin */
    short	o_lastused;	/* day of year last used */
    short	o_status;	/* playing, cloaked, etc. */
    short	o_tampered;	/* decree'd, etc. flag */
    double	o_1scratch,
		o_2scratch;	/* variables used for decree, player battle */
    bool	o_blindness;	/* blindness */
    int		o_notused;   	/* not used */
    long	o_age;		/* age in seconds */
    short	o_degenerated;	/* age/2500 last degenerated */
    short	o_istat;	/* used for inter-terminal battle */
#ifdef PHANTPLUS
    short	o_lives;
#endif
    };
