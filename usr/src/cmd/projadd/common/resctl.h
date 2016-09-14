#ifndef _PROJENT_RCTL_H
#define _PROJENT_RCTL_H


/*
 * Put includes here if needed to be.
 */

#ifdef  __cplusplus
extern "C" {
#endif


#define RESCTL_TYPE_UNKWN 0x00
#define RESCTL_TYPE_BYTES 0x01
#define RESCTL_TYPE_SCNDS 0x02
#define RESCTL_TYPE_COUNT 0x03

#define RESCTL_PRIV_PRIVE 0x01
#define RESCTL_PRIV_PRIVD 0x02
#define RESCTL_PRIV_BASIC 0x04
#define RESCTL_PRIV_ALLPR 0x07

#define RESCTL_ACTN_NONE  0x01
#define RESCTL_ACTN_DENY  0x02
#define RESCTL_ACTN_SIGN  0x04
#define RESCTL_ACTN_ALLA  0x07

#define RESCTL_SIG_ABRT   0x01
#define RESCTL_SIG_XRES   0x02
#define RESCTL_SIG_HUP    0x04
#define RESCTL_SIG_STOP   0x08
#define RESCTL_SIG_TERM   0x10
#define RESCTL_SIG_KILL   0x20
#define RESCTL_SIG_XFSZ   0x40
#define RESCTL_SIG_XCPU   0x80

#define RESCTL_SIG_CMN    0x3f
#define RESCTL_SIG_ALL    0xff

typedef struct sig_s {
        char    sig[6];
        int     mask;
} sig_t;

#define SIGS_CNT        16
extern sig_t sigs[SIGS_CNT];

typedef struct resctlrule_s {
        uint64_t resctl_max;
        uint8_t  resctl_type;
        uint8_t  resctl_privs;
        uint8_t  resctl_action;
        uint8_t  resctl_sigs;
} resctlrule_t;

typedef struct resctl_info_s {
	unsigned long long value;
	int flags;
} resctl_info_t;

extern int resctl_pool_exist(char *);
extern int resctl_get_info(char *, resctl_info_t *);
extern void resctl_get_rule(resctl_info_t *, resctlrule_t *);

#ifdef  __cplusplus
}
#endif
#endif /* _PROJENT_RCTL_H */
