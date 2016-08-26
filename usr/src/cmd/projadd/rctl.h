#ifndef _PROJENT_RCTL_H
#define _PROJENT_RCTL_H


/*
 * Put includes here if needed to be.
 */

#ifdef  __cplusplus
extern "C" {
#endif


#define RCTL_TYPE_UNKWN 0x00
#define RCTL_TYPE_BYTES 0x01
#define RCTL_TYPE_SCNDS 0x02
#define RCTL_TYPE_COUNT 0x03

#define RCTL_PRIV_PRIV  0x01
#define RCTL_PRIV_PRIVD 0x02
#define RCTL_PRIV_BASIC 0x04
#define RCTL_PRIV_ALL   0x07

#define RCTL_ACTN_NONE  0x01
#define RCTL_ACTN_DENY  0x02
#define RCTL_ACTN_SIG   0x04
#define RCTL_ACTN_ALL   0x07

#define RCTL_SIG_ABRT   0x01
#define RCTL_SIG_XRES   0x02
#define RCTL_SIG_HUP    0x04
#define RCTL_SIG_STOP   0x08
#define RCTL_SIG_TERM   0x10
#define RCTL_SIG_KILL   0x20
#define RCTL_SIG_XFSZ   0x40
#define RCTL_SIG_XCPU   0x80

#define RCTL_SIG_CMN    0x3f
#define RCTL_SIG_ALL    0xff

typedef struct sig_s {
        char    sig[6];
        int     mask;
} sig_t;

typedef struct rctlrule_s {
        uint64_t rtcl_max;
        uint8_t  rtcl_type;
        uint8_t  rctl_privs;
        uint8_t  rctl_action;
        uint8_t  rctl_sigs;
} rctlrule_t;

typedef struct rctl_info_s {
	unsigned long long value;
	int flags;
} rctl_info_t;


extern int rctl_get_info(char *, rctl_info_t *);
extern void rctl_get_rule(rctl_info_t *, rctlrule_t *);

#ifdef  __cplusplus
}
#endif
#endif /* _PROJENT_RCTL_H */
