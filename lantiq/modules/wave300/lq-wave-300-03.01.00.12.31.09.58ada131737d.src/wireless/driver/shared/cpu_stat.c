/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"

#ifdef MTCFG_CPU_STAT

#include "cpu_stat.h"

typedef struct
{
  cpu_stat_track_id_e id;
  const char*         name;
} cpu_stat_track_info_t;

static const cpu_stat_track_info_t cpu_stat_tracks[] = 
{
  { CPU_STAT_ID_NONE,                 "DISABLED"          },
  { CPU_STAT_ID_ISR,                  "IRQ Handler"       },
  { CPU_STAT_ID_TX_OS,                "TX Path OS --> SQ" },
  { CPU_STAT_ID_TX_FWD,               "TX Path RX --> SQ" },
  { CPU_STAT_ID_TX_FLUSH,             "TX Path SQ --> HW" },
  { CPU_STAT_ID_TX_HW,                "TX Path HW"        },
  { CPU_STAT_ID_RX_DAT,               "RX Data Path"      },
  { CPU_STAT_ID_RX_NDAT,              "RX Non-data Path"  },
  { CPU_STAT_ID_RX_EMPTY,             "RX Empty BD Path"  },
  { CPU_STAT_ID_RX_HW,                "RX Path HW"        },
  { CPU_STAT_ID_LAST,                 "UNKNOWN#"          } 
};

static const cpu_stat_track_info_t *
_CPU_STAT_GET_INFO(cpu_stat_track_id_e id)
{
  const cpu_stat_track_info_t *res = NULL;
  int                          i   = 0;

  for (; i < ARRAY_SIZE(cpu_stat_tracks); i++) {
    res = &cpu_stat_tracks[i];
    if (res->id == id)
      break;
    res = NULL;
  }

  return res;
}

const char*
_CPU_STAT_GET_NAME (cpu_stat_track_id_e id)
{
  const cpu_stat_track_info_t *info = _CPU_STAT_GET_INFO(id);

  return info?info->name:NULL;
}

void
_CPU_STAT_GET_NAME_EX (cpu_stat_track_id_e id,
                       char               *buf,
                       uint32              size)
{
  const char* name = _CPU_STAT_GET_NAME(id);
  if (name) {
    strncpy(buf, name, size);
  }
  else {
    snprintf(buf, size, "UNKNOWN#%03d", id);
  }
}

cpu_stat_t g_mtlk_cpu_stat_obj;

#endif
