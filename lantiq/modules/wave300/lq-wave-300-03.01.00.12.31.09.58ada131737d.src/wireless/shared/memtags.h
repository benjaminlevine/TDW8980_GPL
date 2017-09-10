/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
* $Id$
*
* This file contains memory tags for driver's 
* memory allocations. Basic rule is to have 
* separate tag for every logically connected 
* set of allocations. Ideally, separate tag 
* for every place that allocates memory.
*
* Copyright (c) 2006-2007 Metalink Broadband (Israel)
*
* Written by: Dmitry Fleytman
*
*/

#ifndef _MTLK_MEMTAGS_H_
#define _MTLK_MEMTAGS_H_

#define MTLK_MEM_TAG_BSS_CACHE          'cbTM'
#define MTLK_MEM_TAG_BSS_COUNTRY_IE     'ciTM'
#define MTLK_MEM_TAG_SCAN_CACHE         'csTM'
#define MTLK_MEM_TAG_SCAN_VECTOR        'vsTM'
#define MTLK_MEM_TAG_TXMM_STORAGE       'stTM'
#define MTLK_MEM_TAG_TXMM_QUEUE         'qtTM'
#define MTLK_MEM_TAG_TXMM_HISTORY       'htTM'
#define MTLK_MEM_TAG_CANDIDATE          'ncTM'
#define MTLK_MEM_TAG_DPWI_CONTEXT       'cdTM'
#define MTLK_MEM_TAG_CMD                'mcTM'
#define MTLK_MEM_TAG_CMD_ARGS           'acTM'
#define MTLK_MEM_TAG_CMD_DATA           'dcTM'
#define MTLK_MEM_TAG_CMD_TEMP           'pcTM'
#define MTLK_MEM_TAG_PROC_CMD           'cpTM'
#define MTLK_MEM_TAG_REG_LIMIT          'lrTM'
#define MTLK_MEM_TAG_REG_DOMAIN         'drTM'
#define MTLK_MEM_TAG_REG_CLASS          'crTM'
#define MTLK_MEM_TAG_REG_CHANNEL        'hrTM'
#define MTLK_MEM_TAG_REG_HW_LIMITS      'shTM'
#define MTLK_MEM_TAG_REG_HW_LIMIT       'lhTM'
#define MTLK_MEM_TAG_REG_RC_LIMIT       'mrTM'
#define MTLK_MEM_TAG_ANTENNA_GAIN       'gaTM'
#define MTLK_MEM_TAG_EEPROM             'eeTM'
#define MTLK_MEM_TAG_CORE               'ocTM'
#define MTLK_MEM_TAG_HW                 'whTM'
#define MTLK_MEM_TAG_TPC_DATA           'dtTM'
#define MTLK_MEM_TAG_TPC_POINT          'ptTM'
#define MTLK_MEM_TAG_TPC_DATA_ARRAY     'atTM'
#define MTLK_MEM_TAG_AOCS_ENTRY         'eaTM'
#define MTLK_MEM_TAG_AOCS_TABLE_ENTRY1  '1aTM'
#define MTLK_MEM_TAG_AOCS_TABLE_ENTRY2  '2aTM'
#define MTLK_MEM_TAG_AOCS_TABLE_ENTRY3  '3aTM'
#define MTLK_MEM_TAG_AOCS_TABLE_ENTRY4  '4aTM'
#define MTLK_MEM_TAG_AOCS_PENALTY       'paTM'
#define MTLK_MEM_TAG_AOCS_CHANNELS      'caTM'
#define MTLK_MEM_TAG_AOCS_TABLE         'taTM'
#define MTLK_MEM_TAG_AOCS_RESTR_CHNL    'raTM'
#define MTLK_MEM_TAG_AOCS_40MHZ_INT     'iaTM'
#define MTLK_MEM_TAG_BDR                'dbTM'
#define MTLK_MEM_TAG_FILE1              '1fTM'
#define MTLK_MEM_TAG_FILE2              '2fTM'
#define MTLK_MEM_TAG_RESOURCES          'srTM'
#define MTLK_MEM_TAG_MINIPORT           'pmTM'
#define MTLK_MEM_TAG_WORKITEM           'iwTM'
#define MTLK_MEM_TAG_PCI                'pci '
#define MTLK_MEM_TAG_SEND_QUEUE         'sqTM'
#define MTLK_MEM_TAG_SEND_QUEUE_CLONE   'sqCL'
#define MTLK_MEM_TAG_MIB_VALUES         'mibV'
#define MTLK_MEM_TAG_MIB                'mib '
#define MTLK_MEM_TAG_PROC               'proc'
#define MTLK_MEM_TAG_L2NAT              'l2na'
#define MTLK_MEM_TAG_IOCTL              'ioct'
#define MTLK_MEM_TAG_MCAST              'mcas'
#define MTLK_MEM_TAG_DEBUG_DATA         'dbg '
#define MTLK_MEM_TAG_COMPAT             'comp'
#define MTLK_MEM_TAG_LOGGER             'log '
#define MTLK_MEM_TAG_EXTENSION          'cemp'
#define MTLK_MEM_TAG_AOCS_HISTORY       'haTM'
#define MTLK_MEM_TAG_OBJPOOL            'poTM'
#define MTLK_MEM_TAG_IRB                'irbs'
#define MTLK_MEM_TAG_IRBCALL            'irbc'
#define MTLK_MEM_TAG_IRBNOTIFY          'irbn'
#define MTLK_MEM_TAG_HASH               'hash'
#define MTLK_MEM_TAG_RFMGMT             'rfmg'
#define MTLK_MEM_TAG_CONTAINER          'cont'
#define MTLK_MEM_TAG_EQ                 'eq  '
#define MTLK_MEM_TAG_PROGMODEL          'prgm'
#define MTLK_MEM_TAG_IRBPINGER          'irbp'
#define MTLK_MEM_TAG_CLI_CLT            'clic'
#define MTLK_MEM_TAG_CLI_SRV            'clis'
#define MTLK_MEM_TAG_STRTOK             'stok'
#define MTLK_MEM_TAG_ARGV_PARSER        'argv'
#define MTLK_MEM_TAG_UTF                'Mutf'
#define MTLK_MEM_TAG_TPC4               'tpc4'
#define MTLK_MEM_TAG_COC                'coc '

#endif //_MTLK_MEMTAGS_H_

