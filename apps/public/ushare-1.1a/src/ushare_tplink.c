/*  Copyright(c) 2009-2011 Shenzhen TP-LINK Technologies Co.Ltd.
 *
 * file		ushare_tplink.c
 * brief	tplink ��˾��ص�һЩʵ��.	
 * details	
 *
 * author	LI CHENGLONG
 * version	1.0.0
 * date		24Nov11
 *
 * history 	\arg  1.0.0, 24Nov11, LI CHENGLONG, Create the file.	
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>

#include "list_template.h"

#include "upnp.h"
#include "upnptools.h"
#include "TimerThread.h"

#include "ushare.h"
#include "ushare_tplink.h"
#include "metadata.h"
#include "config.h"
#include "cfgparser.h"
#include "trace.h"
 
/**************************************************************************************************/
/*                                           DEFINES                                              */
/**************************************************************************************************/
#define USB_DISK_DIRECTORY	"/var/usbdisk"
#define USHARE_MAX_FOLDER_NUM	32
#define USBVM 	"usbvm"

/*���Ŀ¼ʱ�Ƿ������Ϣ��, Added by LI CHENGLONG , 2011-Dec-18.*/
#define NO_SYNC_MTREE	0
#define	SYNC_MTREE	1
/* Ended by LI CHENGLONG , 2011-Dec-18.*/


/* 
 * brief: Added by LI CHENGLONG, 2011-Dec-22.
 *		  ʹ��ͬ������meta tree ��content list ��blackredtree ������ȫ�����ؽ�.
 */
#define USHARE_USE_SYNC 	0
/**************************************************************************************************/
/*                                           TYPES                                                */
/**************************************************************************************************/

/**************************************************************************************************/
/*                                           EXTERN_PROTOTYPES                                    */
/**************************************************************************************************/
/* Added by LI CHENGLONG , 2011-Nov-24.*/
extern ushare_t *ut;
extern TimerThread gTimerThread;
extern int uppn_setMPoolAttr(ThreadPoolAttr *newAttr);/*in upnp lib*/

/**************************************************************************************************/
/*                                           LOCAL_PROTOTYPES                                     */
/**************************************************************************************************/
/* 
 *--------------------------------------------------------------------------------------------------
 *�����Ǵ���CMM������ĺ�������
 *--------------------------------------------------------------------------------------------------	  
 */

/* 
 * fn		static BOOL usHandleInitInfoReq(const DMS_INIT_INFO_MSG *pIF) 
 * brief	���ݳ�ʼ��������Ϣ����Ushare�����е�ȫ������.	
 *
 * param[in]	pIF		ָ��DMS_INIT_INFO_MSG ������Ϣ��ָ��
 *
 * return		BOOL	����ֵ
 * retval		TRUE	���óɹ�
 *				FALSE	����ʧ��
 *
 * note	written by  24Nov11, LI CHENGLONG	
 */
static BOOL usHandleInitInfoReq(const DMS_INIT_INFO_MSG *pIF);

/* 
 * fn		static BOOL usHandleInitFolderReq(const DMS_FOLDER_INFO *pFI) 
 * brief	�����ʼ��ʱ���ݹ����Ĺ���Ŀ¼��Ϣ.	
 * details	��Ϊbuf�Ĵ�Сԭ��,Ŀ¼����Ϣ��Ҫ�ֿ�����.	
 *
 * param[in]	pFI		�����ļ��е���Ϣ.
 *
 * return		BOOL	����ֵ
 * retval		TRUE	���óɹ�
 *				FALSE	����ʧ��
 *
 * note	written by  16Dec11, LI CHENGLONG	
 */
static BOOL usHandleInitFolderReq(const DMS_OP_FOLDER_MSG *pFI);

/* 
 * fn		static BOOL usHandleAddNewShareFolderReq(const DMS_OP_FOLDER_MSG *pFolder) 
 * brief	���һ������Ŀ¼.	
 *
 * param[in]	pFolder		ָ��һ������Ŀ¼��Ϣ.
 *
 * return		BOOL		����ֵ
 * retval		TRUE		�ɹ�
 *				FALSE		ʧ��
 *
 * note	written by  24Nov11, LI CHENGLONG	
 */
static BOOL usHandleAddNewShareFolderReq(const DMS_OP_FOLDER_MSG *pFolder);

/* 
 * fn		static BOOL usHandleDelShareFolderReq(const DMS_OP_FOLDER_MSG *pFolder) 
 * brief	ɾ��һ������Ŀ¼.		
 *
 * param[in]	pFolder		ָ��һ������Ŀ¼��Ϣ.
 *
 * return		BOOL		����ֵ
 * retval		TRUE		�ɹ�
 *				FALSE		ʧ��
 *
 * note	written by  24Nov11, LI CHENGLONG	
 */
static BOOL usHandleDelShareFolderReq(const DMS_OP_FOLDER_MSG *pFolder);

/* 
 * fn		static BOOL usHandleOperateFolderReq(const DMS_OP_FOLDER_MSG *pFolder)
 * brief	��ӻ�ɾ������Ŀ¼ʱ�Ĵ�����.	
 * details	������� �� ɾ�� Ŀ¼.	
 *
 * param[in]	pFolder		ָ��Ŀ¼������ָ��.
 *
 * return		BOOL		����ֵ
 * retval		TRUE		������ӻ�ɾ��Ŀ¼��Ϣ�ɹ�
 *				FALSE		������ӻ�ɾ��Ŀ¼��Ϣʧ��
 *
 * note	written by  16Dec11, LI CHENGLONG	
 */
static BOOL usHandleOperateFolderReq(const DMS_OP_FOLDER_MSG *pFolder);

/* 
 * fn		static BOOL usHandleReloadReq(const DMS_RELOAD_MSG *pReload) 
 * brief	���û����øı���͸���Ϣ��ushare����,���������յĴ�����.	
 *
 * param[in]	pReload			ָ��reload ��Ϣ�ṹ.
 *
 * return		BOOL			����ֵ
 * retval		TRUE			����reload ��Ϣ�ɹ�
 *				FALSE			����reload��Ϣʧ��
 *
 * note	written by  16Dec11, LI CHENGLONG	
 */
static BOOL usHandleReloadReq(const DMS_RELOAD_MSG *pReload);

/* 
 * fn		static BOOL usHandleManualScanReq() 
 * brief	���ֶ�ɨ�蹲��Ŀ¼.	
 *
 * return	BOOL		����ֵ
 * retval	TRUE		���óɹ�
 *			FALSE		����ʧ��
 *
 * note	written by  24Nov11, LI CHENGLONG	
 */
static BOOL usHandleManualScanReq();

/* 
 * fn		static BOOL usHandleDeviceStateChange()
 * brief	when usb device's state changed,rebuild all the content list and metadata list
 *
 * param[in]	
 * param[out]	
 *
 * return	 BOOL
 * retval	TRUE :rebuild successful
 		FALSE:rebuild failed
 *
 * note		
 */
static BOOL usHandleDeviceStateChange();

/* 
 *--------------------------------------------------------------------------------------------------
 *������һЩ�õ���static��������
 *--------------------------------------------------------------------------------------------------
 */

/* 
 * fn		static BOOL ushareAutoScanWorker()
 * brief	ushare �����е��Զ�ɨ��job	
 *
 *
 * return		BOOL		����ֵ
 * retval		TRUE		�ɹ�
 *				FALSE		ʧ��
 *
 * note	written by  25Nov11, LI CHENGLONG	
 */
static BOOL ushareAutoScanWorker();

/* 
 * fn		static int usFolderRelationship(const char *dirA, const char* dirB)
 * brief	�ж�����Ŀ¼�Ĺ�ϵ	
 * details	port form HXB ���,���ӹ�ϵ,�޹�ϵ.	
 *
 * param[in]	dirA		Ŀ¼A��·��
 * param[in]	dirB		Ŀ¼B��·��
 *
 * return		int			����Ŀ¼�Ĺ�ϵ
 * retval		-1			error
 *				0			����Ŀ¼���
 *				1			dirA��dirB����Ŀ¼
 *				2			dirA��dirB�ĸ�Ŀ¼
 *				3			dirA��dirBû�и��ӹ�ϵ
 *
 * note	written by  05Jan12, LI CHENGLONG	
 */
static int usFolderRelationship(const char *dirA, const char* dirB);

/**************************************************************************************************/
/*                                           VARIABLES                                            */
/**************************************************************************************************/
/*Ushare ������Ϣ��صĽṹ, Added by LI CHENGLONG , 2011-Nov-24.*/
CMSG_FD g_usFd;
CMSG_BUFF g_usBuff;
/* Ended by LI CHENGLONG , 2011-Nov-24.*/

/**************************************************************************************************/
/*                                           LOCAL_FUNCTIONS                                      */
/**************************************************************************************************/
/* 
 *--------------------------------------------------------------------------------------------------
 *�����Ǵ���CMM������ĺ���ʵ��
 *--------------------------------------------------------------------------------------------------
 */
 
/* 
 * fn		static BOOL usHandleInitInfoReq(const DMS_INIT_INFO_MSG *pIF) 
 * brief	���ݳ�ʼ��������Ϣ����Ushare�����е�ȫ������.	
 *
 * param[in]	pIF		ָ��USHARE_INIT_INFO_MSG ������Ϣ��ָ��
 *
 * return		BOOL	����ֵ
 * retval		TRUE	���óɹ�
 *				FALSE	����ʧ��
 *
 * note	written by  24Nov11, LI CHENGLONG	
 */
static BOOL usHandleInitInfoReq(const DMS_INIT_INFO_MSG *pIF)
{
	assert(pIF != NULL);

	if (pIF == NULL)
	{
		return FALSE;
	}

	/*just copy it, Added by LI CHENGLONG , 2011-Nov-24.*/
	memcpy(&(ut->initInfo), pIF, sizeof(DMS_INIT_INFO_MSG));
	ut->initInfo.serverState = pIF->serverState ? 1 : 0;
	ut->initInfo.scanFlag = pIF->scanFlag ? 1 : 0;
	ut->initInfo.scanInterval = pIF->scanInterval > 0 ? pIF->scanInterval : US_SCAN_DFINTERVAL;
	ut->initInfo.folderCnt = pIF->folderCnt > 0 ? pIF->folderCnt : 0;
	
 	/*timer thread  not executing, no need mutex, Added by LI CHENGLONG , 2011-Nov-25.*/
	ut->scan.scanFlag= ut->initInfo.scanFlag;
	ut->scan.scanInterval = ut->initInfo.scanInterval;
	ut->scan.counter = ut->initInfo.scanInterval;


	if (strlen(ut->initInfo.serverName) == 0)
	{
		snprintf(ut->initInfo.serverName, USHARE_MEDIA_SERVER_SERVERNAME_L, "%s", US_DEF_NAME);
	}
 	/* Ended by LI CHENGLONG , 2011-Nov-25.*/

	USHARE_DEBUG("init ut->initInfo and ut->scan data struct END.\n ");
	
	return TRUE;
}

/* 
 * fn		static BOOL usHandleInitFolderReq(const DMS_OP_FOLDER_MSG *pFI) 
 * brief	�����ʼ��ʱ���ݹ����Ĺ���Ŀ¼��Ϣ.	
 * details	��Ϊbuf�Ĵ�Сԭ��,Ŀ¼����Ϣ��Ҫ�ֿ�����.	
 *
 * param[in]	pFI			�����ļ��е���Ϣ.
 *
 * return		BOOL		����ֵ
 * retval		TRUE		���óɹ�
 *				FALSE		����ʧ��
 *
 * note	written by  16Dec11, LI CHENGLONG	
 */
static BOOL usHandleInitFolderReq(const DMS_OP_FOLDER_MSG *pFI)
{
	USHARE_FOLDER_INFO *pNew = NULL;
	
	assert(pFI != NULL);

	if (pFI == NULL)
	{
		return FALSE;
	}

	if (pFI->op != DMS_INIT_FOLDER)
	{
		log_error("not init folder req.");
		return FALSE;
	}
	
	pNew = (USHARE_FOLDER_INFO *)malloc(sizeof (USHARE_FOLDER_INFO));
	if (pNew == NULL)
	{
		log_error("malloc failed.");
		return FALSE;
	}

	memcpy(pNew, &pFI->folder, sizeof(pFI->folder));
	pNew->list.next = pNew->list.prev = NULL;
	
	list_add(&(pNew->list), &ut->initInfo.folderList);
	
	USHARE_DEBUG("handle init folder request END\n");

	return TRUE;

}
 
/* 
 * fn		static BOOL usHandleAddNewShareFolderReq(const DMS_OP_FOLDER_MSG *pFolder) 
 * brief	���һ������Ŀ¼.	
 * details		
 *
 * param[in]	pFolder		ָ��һ������Ŀ¼��Ϣ.
 *
 * return		BOOL		����ֵ
 * retval		TRUE		�ɹ�
 *				FALSE		ʧ��
 *
 * note	written by  24Nov11, LI CHENGLONG	
 */
static BOOL usHandleAddNewShareFolderReq(const DMS_OP_FOLDER_MSG *pFolder)
{
	BOOL addFlag = TRUE;
	struct list_head *pos = NULL;
	USHARE_FOLDER_INFO *pNew = NULL;
	USHARE_FOLDER_INFO *pEntry = NULL;

	assert(pFolder != NULL);

	if (pFolder == NULL)
	{
		return FALSE;
	}

	/*ֻ��·����ͬ�ſ������, Added by LI CHENGLONG , 2011-Dec-26.*/
	list_for_each(pos, &ut->initInfo.folderList)
	{
		pEntry = list_entry(pos, USHARE_FOLDER_INFO, list);

		if (strcmp(pEntry->path, pFolder->folder.path) == 0)
		{	
			addFlag = FALSE;
			break;
		}
	}

	if (addFlag)
	{
		/*update config first, Added by LI CHENGLONG , 2011-Dec-18.*/
		pNew = (USHARE_FOLDER_INFO *)malloc(sizeof (USHARE_FOLDER_INFO));
		if (pNew == NULL)
		{
			log_error("malloc failed!");
			return FALSE;
		}

		memcpy(pNew, &pFolder->folder, sizeof(pFolder->folder));
		pNew->list.next = pNew->list.prev = NULL;
		list_add(&(pNew->list), &ut->initInfo.folderList);
		
#if USHARE_USE_SYNC == 1
		USHARE_DEBUG("add content %s,%s use sync...\n", 
					 pFolder->folder.path, 
					 pFolder->folder.dispName);

		if (pFolder->folder.path && pFolder->folder.dispName);
		{
			/* 
			 * brief: Added by LI CHENGLONG, 2011-Dec-18.
			 *		  TODO:
			 *			1.sync
			 *			2.add		
			 */
			ut->contentlist = ushare_sync_mtree(ut->contentlist);
		
			ut->contentlist = ushare_content_add(ut->contentlist, 
												 pFolder->folder.path, 
												 pFolder->folder.dispName, 
												 SYNC_MTREE);
		}
#else
	ushare_rebuild_all();/*just rebuild it, more simple than sync.*/
#endif /* #if USHARE_USE_SYNC == 1 */
	}
	
	return TRUE;
}

/* 
 * fn		static BOOL usHandleDelShareFolderReq(const DMS_OP_FOLDER_MSG *pFolder) 
 * brief	ɾ��һ������Ŀ¼.		
 *
 * param[in]	pFolder		ָ��һ������Ŀ¼��Ϣ.
 *
 * return		BOOL		����ֵ
 * retval		TRUE		�ɹ�
 *				FALSE		ʧ��
 *
 * note	written by  24Nov11, LI CHENGLONG	
 */
static BOOL usHandleDelShareFolderReq(const DMS_OP_FOLDER_MSG *pFolder)
{
	struct list_head *pos = NULL;
	USHARE_FOLDER_INFO *pEntry = NULL;
	
	assert(pFolder != NULL);
	
	if (pFolder == NULL)
	{
		return FALSE;
	}

	/*find it out, where you are? ^_^, update config first, Added by LI CHENGLONG , 2011-Dec-18.*/
	list_for_each(pos, &ut->initInfo.folderList)
	{
		pEntry = list_entry(pos, USHARE_FOLDER_INFO, list);
		
		if (strcmp(pEntry->path, pFolder->folder.path) == 0)
		{			
			list_del(pos);
			free(pEntry);
			break;
		}
	}
	
#if USHARE_USE_SYNC == 1
	USHARE_DEBUG("content %s,%s delete use sync\n",
	 			 pFolder->folder.path, 
				 pFolder->folder.dispName);

	ut->contentlist = ushare_content_del(ut->contentlist, 
										 pFolder->folder.path, 
										 pFolder->folder.dispName, 
										 SYNC_MTREE);
#else
	return ushare_rebuild_all();/*just rebuild it, more simple than sync.*/
#endif /*#if USHARE_USE_SYNC == 1*/

	return TRUE;
}

/* 
 * fn		static BOOL usHandleOperateFolderReq(const DMS_OP_FOLDER_MSG *pFolder)
 * brief	��ӻ�ɾ������Ŀ¼ʱ�Ĵ�����.	
 * details	������� �� ɾ�� Ŀ¼.	
 *
 * param[in]	pFolder	ָ��Ŀ¼������ָ��.
 *
 * return		BOOL	����ֵ
 * retval		TRUE	������ӻ�ɾ��Ŀ¼��Ϣ�ɹ�
 *				FALSE	������ӻ�ɾ��Ŀ¼��Ϣʧ��
 *
 * note	written by  16Dec11, LI CHENGLONG	
 */
static BOOL usHandleOperateFolderReq(const DMS_OP_FOLDER_MSG *pFolder)
{
	assert(pFolder != NULL);

	if (pFolder == NULL)
	{
		return FALSE;
	}

	switch (pFolder->op)
	{

	case DMS_ADD_FOLDER:
		return usHandleAddNewShareFolderReq(pFolder);
		break;
		
	case DMS_DEL_FOLDER:
		return usHandleDelShareFolderReq(pFolder);
		break;
		
	/*do not care about the others now, Added by LI CHENGLONG , 2011-Dec-16.*/
	default:
		USHARE_DEBUG("we don't care it,^_^!");
		break;
	}

	return TRUE;
}

/* 
 * fn		static BOOL usHandleReloadReq(const DMS_RELOAD_MSG *pReload) 
 * brief	���û����øı���͸���Ϣ��ushare����,���������յĴ�����.	
 *
 * param[in]	pReload		ָ��reload ��Ϣ�ṹ.
 *
 * return		BOOL		����ֵ
 * retval		TRUE		����reload ��Ϣ�ɹ�
 *				FALSE		����reload��Ϣʧ��
 *
 * note	written by  16Dec11, LI CHENGLONG	
 */
static BOOL usHandleReloadReq(const DMS_RELOAD_MSG *pReload)
{
	assert(pReload != NULL);
	
	if (pReload == NULL)
	{
		return FALSE;
	}
	
//	sem_wait();
	ut->initInfo.serverState = pReload->serverState ? 1 : 0;
	ut->initInfo.scanFlag = pReload->scanFlag ? 1 : 0;
	ut->initInfo.scanInterval = pReload->scanInterval > 0 ?
								pReload->scanInterval : 
								US_SCAN_DFINTERVAL;
	
	ut->scan.scanFlag = ut->initInfo.scanFlag;
	ut->scan.scanInterval = ut->initInfo.scanInterval;
	ut->scan.counter = ut->scan.scanInterval;

//	sem_post();
	
	if (strcmp(pReload->serverName, ut->initInfo.serverName) != 0)
	{
		/*������Ʋ���ͬ, Added by LI CHENGLONG , 2011-Dec-25.*/
		if (strlen(pReload->serverName) != 0)
		{
			snprintf(ut->initInfo.serverName, 
				 	 USHARE_MEDIA_SERVER_SERVERNAME_L, 
					 "%s", 
				 	 pReload->serverName);

			USHARE_DEBUG("name changed restart it...");
			if(pReload->serverState)
			{
				ushareSwitchOnOff(FALSE);
				return ushareSwitchOnOff(TRUE);
			
			}
			else
			{
				return ushareSwitchOnOff(FALSE);
			}
			
		}
		else
		{
			return ushareSwitchOnOff(pReload->serverState > 0 ? TRUE : FALSE);
		}

	}
	else
	{
		return ushareSwitchOnOff(pReload->serverState > 0 ? TRUE : FALSE);
	}
	
}

/* 
 * fn		static BOOL usHandleManualScanReq() 
 * brief	���ֶ�ɨ�蹲��Ŀ¼.	
 *
 * return	BOOL		����ֵ
 * retval	TRUE		���óɹ�
 *			FALSE		����ʧ��
 *
 * note	written by  24Nov11, LI CHENGLONG	
 */
static BOOL usHandleManualScanReq()
{
/* 
 * brief: Added by LI CHENGLONG, 2011-Dec-18.
 *		  just rebuild all ,so what about big big disk? just pray ^_^.
 *		  maybe use sqlite... in the future.
 */

	/* Modify by chz, 
	 * Fix the bug: the playing would interrupted once scan with deleting
	 * the metadata list. So just sync the metadata list, don't rebuild it. */
	//return ushare_rebuild_all();

	content_free(ut->contentlist);
	ut->contentlist = NULL;

	build_content_list(ut);
	sync_metadata_list(ut);

	return TRUE;
	/* end modify */
}


/* 
 * fn		static BOOL usHandleDeviceStateChange()
 * brief	when usb device's state changed,rebuild all the content list and metadata list
 *
 * param[in]	
 * param[out]	
 *
 * return	 BOOL
 * retval	TRUE :rebuild successful
 		FALSE:rebuild failed
 *
 * note		
 */
static BOOL usHandleDeviceStateChange()
{
	return ushare_rebuild_all();
}
/* 
 *--------------------------------------------------------------------------------------------------
 *������һЩ�õ���static����ʵ��
 *--------------------------------------------------------------------------------------------------
 */
/* 
 * fn		static BOOL ushareAutoScanWorker()
 * brief	ushare �����е��Զ�ɨ��job	
 *
 *
 * return		BOOL	����ֵ
 * retval		TRUE	�ɹ�
 *				FALSE	ʧ��
 *
 * note	written by  25Nov11, LI CHENGLONG	
 */
static BOOL ushareAutoScanWorker()
{
	int ret = 0;
	ThreadPoolJob job;

 	/*avoid competition, Added by LI CHENGLONG , 2011-Nov-25.*/
	//sem_wait();
	if (ut->scan.scanFlag == 1)
	{
		if (--ut->scan.counter == 0)
		{	
			USHARE_DEBUG("ushare auto scan timeout, rescan it.\n");
			/* Modify by chz, 
			 * Fix the bug: the playing would interrupted once scan with deleting
			 * the metadata list. So just sync the metadata list, don't rebuild it. */
			//ushare_rebuild_all();
			
			content_free(ut->contentlist);
			ut->contentlist = NULL;

			build_content_list(ut);
			sync_metadata_list(ut);
			/* end modify */
			
			ut->scan.counter = ut->scan.scanInterval;
		}
		
	}
	//sem_post();
 	/* Ended by LI CHENGLONG , 2011-Nov-25.*/

	/*do it again and again are you tired ^_^? Added by LI CHENGLONG , 2011-Dec-26.*/
	TPJobInit( &job, ( start_routine ) ushareAutoScanWorker, NULL);
	TPJobSetFreeFunction( &job, ( free_routine )NULL);
	TPJobSetPriority( &job, MED_PRIORITY );

	ret = TimerThreadSchedule(&gTimerThread,
							  1,
							  REL_SEC,
							  &job,
							  SHORT_TERM,
							  NULL);
	
	if (ret != 0)
	{
		return FALSE;
	}

	return TRUE;
	
}

/* 
 * fn		static int usFolderRelationship(const char *dirA, const char* dirB)
 * brief	�ж�����Ŀ¼�Ĺ�ϵ	
 * details	port form HXB ���,���ӹ�ϵ,�޹�ϵ.	
 *
 * param[in]	dirA		Ŀ¼A��·��
 * param[in]	dirB		Ŀ¼B��·��
 *
 * return		int			����Ŀ¼�Ĺ�ϵ
 * retval		-1			error
 *				0			����Ŀ¼���
 *				1			dirA��dirB����Ŀ¼
 *				2			dirA��dirB�ĸ�Ŀ¼
 *				3			dirA��dirBû�и��ӹ�ϵ
 *
 * note	written by  05Jan12, LI CHENGLONG	
 */
static int usFolderRelationship(const char *dirA, const char* dirB)
{
	int i = 0;
	
	assert((dirA != NULL) && (dirB != NULL));
	
	if ((NULL == dirA) || (NULL == dirB))
	{
		return -1;
	}

	if(strlen(dirA) == strlen(dirB))
	{
		for (i = 0; i < strlen(dirA); i++)
		{
			if(dirA[i] != dirB[i])
			{
				return 3;
			}
		}
		return 0;
	}
	else if (strlen(dirA) > strlen(dirB))
	{		
		for (i = 0; i < strlen(dirB); i++)
		{
			if(dirA[i] != dirB[i])
			{
				return 3;
			}
		}
		return 1;
	}
	else
	{
		for (i = 0; i < strlen(dirA); i++)
		{
			if(dirA[i] != dirB[i])
			{
				return 3;
			}
		}
		return 2;
	}
	
}

/**************************************************************************************************/
/*                                           PUBLIC_FUNCTIONS                                     */
/**************************************************************************************************/
/* 
 *--------------------------------------------------------------------------------------------------
 *������һЩpublic����ʵ��,ushare�����������ط����õ�.
 *--------------------------------------------------------------------------------------------------
 */

/* 
 * fn		int ushareProcessInitInfo()
 * brief	ushare ���̽��ճ�ʼ��������Ϣ.	
 *
 * return		BOOL		����ֵ
 * retval		TRUE		�ɹ�
 *				FALSE		ʧ��	
 *
 * note	written by  24Nov11, LI CHENGLONG	
 */
BOOL ushareProcessInitInfo()
{
	int sRet = 0;
	fd_set usFds;
	BOOL iStep1_Done = FALSE;
	BOOL iStep2_Done = FALSE;
	unsigned int folderCnt = 0;


	while (!((iStep1_Done == TRUE) && (iStep2_Done == TRUE)))
	{	
		/*listen ��Ϣ Added by LI CHENGLONG , 2011-Nov-02.*/
		memset(&g_usBuff, 0 , sizeof(CMSG_BUFF));

		FD_ZERO(&usFds);
   		FD_SET(g_usFd.fd, &usFds);

		USHARE_DEBUG("waiting for message...\n");
	    sRet = select(g_usFd.fd + 1, &usFds, NULL, NULL, NULL); 
		if (sRet == 0)
		{	
			/*timeout. Added by LI CHENGLONG , 2011-Nov-24.*/
			continue;
		}
		else if (sRet < 0)
		{
			if (sRet != EINTR && sRet != EAGAIN)
			{
				/*error and neither EINTR nor EAGAIN Added by LI CHENGLONG , 2011-Nov-24.*/
				return FALSE;
			}

			continue;
		}
		else if (FD_ISSET(g_usFd.fd, &usFds))
		{
			msg_recv(&g_usFd, &g_usBuff);

			switch (g_usBuff.type)
			{
			case CMSG_DLNA_MEDIA_SERVER_INIT:
				USHARE_DEBUG("received CMSG_DLNA_MEDIA_SERVER_INIT message.\n");
				if ((iStep1_Done == TRUE) || (iStep2_Done == TRUE))
				{
					/*order error in recving msg. Added by LI CHENGLONG , 2011-Dec-16.*/	
					break;
				}
				if (TRUE != usHandleInitInfoReq((DMS_INIT_INFO_MSG *)(g_usBuff.content)))
				{
					/*handle init info error, Added by LI CHENGLONG , 2011-Dec-16.*/
					return FALSE;
				}
				
				iStep1_Done = TRUE;
				if (ut->initInfo.folderCnt == 0)
				{
					iStep2_Done = TRUE;
				}

				break;

			case CMSG_DLNA_MEDIA_SERVER_OP_FOLDER:
				USHARE_DEBUG("received CMSG_DLNA_MEDIA_SERVER_OP_FOLDER message.\n");
				if (iStep1_Done != TRUE)
				{
					/*order error in recving msg. Added by LI CHENGLONG , 2011-Dec-16.*/
					return FALSE;
				}
				
				if (folderCnt >= ut->initInfo.folderCnt)
				{
					return FALSE;	
				}
				
				if (TRUE != usHandleInitFolderReq((DMS_OP_FOLDER_MSG *)(g_usBuff.content)))
				{
					return FALSE;
				}
				
				folderCnt++;
				
				if (folderCnt == ut->initInfo.folderCnt)
				{
					iStep2_Done = TRUE;
				}
				break;

			/*unknown msg, not supported yet Added by LI CHENGLONG , 2011-Nov-24.*/		
			default:
				break;
			}

		}

	}

	if ((iStep1_Done == TRUE) && (iStep2_Done == TRUE))
	{
		USHARE_DEBUG("handle initInfo end!\n");
		return TRUE;
	}

	/*should not be here, Added by LI CHENGLONG , 2011-Nov-24.*/
	return FALSE;
}

/* 
 * fn		int ushareProcessUserRequest()
 * brief	ushare ���̼������е��û�����,�����д���.	
 *
 * return	BOOL		����ֵ
 * retval	FALSE		ʧ��
 *			TRUE		�ɹ�
 *
 * note	written by  24Nov11, LI CHENGLONG	
 */
BOOL ushareProcessUserRequest()
{
	int sRet = 0;
	fd_set usFds;

	while(TRUE)
	{	
		/*listen ��Ϣ Added by LI CHENGLONG , 2011-Nov-02.*/
		memset(&g_usBuff, 0 , sizeof(CMSG_BUFF));

		FD_ZERO(&usFds);
   		FD_SET(g_usFd.fd, &usFds);
		
		USHARE_DEBUG("waiting for user request messages...\n");
	    sRet = select(g_usFd.fd + 1, &usFds, NULL, NULL, NULL); 
		if (sRet == 0)
		{	
			/*timeout. Added by LI CHENGLONG , 2011-Nov-24.*/
			continue;
		}
		else if (sRet < 0)
		{
			if ((sRet != EINTR) && (sRet != EAGAIN))
			{
				/*error and not eintr,what happened! Added by LI CHENGLONG , 2011-Nov-24.*/
				return FALSE;
			}

			continue;
		}
		else if (FD_ISSET(g_usFd.fd, &usFds))
		{
			msg_recv(&g_usFd, &g_usBuff);

			switch (g_usBuff.type)
			{
			case CMSG_DLNA_MEDIA_SERVER_RELOAD:
				USHARE_DEBUG("received CMSG_DLNA_MEDIA_SERVER_RELOAD message.\n");
				if (TRUE != usHandleReloadReq((DMS_RELOAD_MSG *)(g_usBuff.content)))
				{
					/*need exit? just ignore it  Added by LI CHENGLONG , 2011-Dec-22.*/
					USHARE_DEBUG("handle reload message error\n");
				}
				break;

			case CMSG_DLNA_MEDIA_SERVER_OP_FOLDER:
				USHARE_DEBUG("received CMSG_DLNA_MEDIA_SERVER_OP_FOLDER message.\n");
				if (TRUE != usHandleOperateFolderReq((DMS_OP_FOLDER_MSG *)(g_usBuff.content)))
				{
					/*need exit? just ignore it  Added by LI CHENGLONG , 2011-Dec-22.*/
					log_error("handle operatefolderreq message error\n");
				}
				break;
				
			case CMSG_DLNA_MEDIA_SERVER_MANUAL_SCAN:
				USHARE_DEBUG("received CMSG_DLNA_MEDIA_SERVER_MANUAL_SCAN message.\n");
				if (TRUE != usHandleManualScanReq())
				{
					/*need exit? just ignore it Added by LI CHENGLONG , 2011-Dec-22.*/
					log_error("handle manual scan message error\n");
				}
				break;
			case CMSG_DLNA_MEDIA_DEVICE_STATE_CHANGE:
				USHARE_DEBUG("received CMSG_DLNA_MEDIA_DEVICE_STATE_CHANGE message.\n");
				if(TRUE !=usHandleDeviceStateChange())	
				{
					log_error("handle device umount message error\n");
				}
				break;
				
			/*unknown msg, not supported yet Added by LI CHENGLONG , 2011-Nov-24.*/
			default :
				USHARE_DEBUG("^_^, not supported yet now!\n");
				break;
				
			}
			
		}
	}

	/*ooh shit, Judgment Day ... should not be here, Added by LI CHENGLONG , 2011-Nov-24.*/
	return FALSE;		
}

/* 
 *--------------------------------------------------------------------------------------------------
 *��������Ŀ¼������ص�һЩ����ʵ��.
 *--------------------------------------------------------------------------------------------------
 */
/* 
 * fn		BOOL ushare_rebuild_all() 
 * brief	���յ��ֶ�ɨ��,���Զ�ɨ��,��/var/usbdisk�¸ı�ʱ�͵��øú���.	
 * details	�ú����ͷ�content_list,�ͷ�metatree,�ͷ�redblack tree;	
 *
 * return	BOOL	����ֵ		
 * retval	0		�ɹ�
 *			-1		ʧ��
 *
 * note	written by  18Dec11, LI CHENGLONG	
 */
BOOL ushare_rebuild_all()
{
#if 0 /* for debug */
	int i = 0;
#endif /* for debug */

	free_metadata_list(ut);
	
	/*free content_list, Added by LI CHENGLONG , 2011-Dec-18.*/
	content_free(ut->contentlist);
	ut->contentlist = NULL;

	build_content_list(ut);
	
	build_metadata_list(ut);

#if 0 /*for debug*/
	USHARE_DEBUG("after rebuild all, content list count=%d.", ut->contentlist->count);
	for (i=0 ; i < ut->contentlist->count ; i++)
	{
		USHARE_DEBUG("ut->contentlist->content[i]=%s,", ut->contentlist->content[i]);
		USHARE_DEBUG("ut->contentlist->dispName[i]=%s\n", ut->contentlist->displayName[i]);
	}
#endif /* for debug */

	return TRUE;
}

/* 
 * fn		content_list *ushare_sync_mtree(content_list *list)
 * brief	����content_list ����,ý����Ϣ���ͺ����.	
 *
 * param[in]	list		content����ͷ.
 *
 * return		content_list*
 * retval		���º��content����ͷ
 *
 * note	written by  18Dec11, LI CHENGLONG	
 */
content_list *ushare_sync_mtree(content_list *list)
/* 
 * brief: Added by LI CHENGLONG, 2011-Dec-18.
 *		  1.walk over content_list.
 *		  2.update metadata tree.
 		  3.update redblack tree.
 */
{
	int cursor = 0;
		
	/*list may be null Added by LI CHENGLONG , 2011-Dec-25.*/
	if (list == NULL)
	{
		return NULL;
	}

	while (cursor < list->count)
	{
		/*�����Ŀ¼���ܷ�����.�ͽ���ɾ����. Added by LI CHENGLONG , 2011-Dec-25.*/
		if (access(list->content[cursor], F_OK) != 0)
		{
			USHARE_DEBUG("list->content[%d]=%s not exist\n", cursor, list->content[cursor]);
			ushare_entry_del(list->content[cursor]);
			list = content_priv_del(list, list->content[cursor]);
		}
		
		cursor++;
	}
	
	return list;	
}

/* 
 * fn		BOOL ushareRebuildMetaList()
 * brief	����ɨ�蹲��Ŀ¼, ���½�����ý����Ϣ��.	
 *
 * return		BOOL	����ֵ
 * retval		TRUE	�ɹ�
 *				FALSE	ʧ��
 *
 * note	written by  24Nov11, LI CHENGLONG	
 */
BOOL ushareRebuildMetaList()
{
	if (ut->contentlist)
	{
		free_metadata_list (ut);
		build_metadata_list (ut);
	}
	
	return TRUE;
}

/* 
 * fn		int ushare_entry_add(int index)
 * brief	��content_list���±�Ϊindex��content��ӵ���Ϣ����redblack����.	
 *
 * param[in]	list	content_list����ͷ.
 * param[in]	index	contnet��content_list�е��±�.
 *
 * return		BOOL	����ֵ
 * retval		TRUE	���ʧ��.
 *				FALSE	��ӳɹ�.
 *
 * note	written by  18Dec11, LI CHENGLONG	
 */
BOOL ushare_entry_add(content_list *list, int index)
{
	char *title = NULL;
	struct upnp_entry_t *entry = NULL;

	if (list == NULL)
	{
		return TRUE;
	}

	if (index >= list->count)
	{
		return TRUE;
	}
	
	USHARE_DEBUG("ushae add entry %s to meta tree begin...\n", list->content[index]);
	if (strlen(list->displayName[index]) > 0)
	{
		title = list->displayName[index];
	}	
	else
	{
	    title = strrchr (list->content[index], '/');
	    if (title)
	      title++;
	    else
	    {
	      /* directly use content directory name if no '/' before basename */
	      title = list->content[index];
	    }
	}

	entry = upnp_entry_new (ut, title, list->content[index], ut->root_entry, -1, true);
	if (!entry)
	{
		/*can't get an new upnp_entry_t, Added by LI CHENGLONG , 2011-Dec-18.*/
		log_error("can't get an new upnp_entry_t\n");
		return FALSE;
	}
	
	upnp_entry_add_child (ut, ut->root_entry, entry);
	
	USHARE_DEBUG("ushae add entry %s to meta tree end...\n", list->content[index]);
	
	USHARE_DEBUG("ushae add container %s to meta tree end...\n", list->content[index]);
	metadata_add_container (ut, entry, list->content[index]);	
	USHARE_DEBUG("ushae add container %s to meta tree end...\n", list->content[index]);

	return TRUE;
}

/* 
 * fn		int ushare_entry_del(const char *fullpath)
 * brief	�ͷ�rootentry�µ�ĳ��Ŀ¼������ý����Ϣռ�õ��ڴ�.	
 *
 * param[in]	fullpath	��ý��Ŀ¼��·��
 *
 * return		BOOL		����ֵ
 * retval		TRUE		�ͷųɹ�
 *				FALSE		�ͷ�ʧ��
 *
 * note	written by  18Dec11, LI CHENGLONG	
 */
BOOL ushare_entry_del(const char *fullpath)
{
	struct upnp_entry_t **ptr = NULL;
	struct upnp_entry_t **childs = NULL;
	
	assert(fullpath != NULL);

	if (fullpath == NULL)
	{
		return TRUE;
	}
	
	USHARE_DEBUG("ushare delete entry %s in meta add blackred tree\n", fullpath);
	for(childs = ut->root_entry->childs; *childs; childs++)
	{
		if (strcmp(fullpath, (*childs)->fullpath) == 0)
		{
			/*because it's allocated by relloc,so do below. Added by LI CHENGLONG , 2011-Dec-18.*/

			USHARE_DEBUG("found %s in meta tree.free begin...\n", fullpath);
			upnp_entry_free(ut, *childs);
			USHARE_DEBUG("found %s in meta tree.free end...\n", fullpath);
			
			USHARE_DEBUG("found %s in meta tree.free childs...\n", fullpath);
			ptr = childs;
			while(*ptr)
			{

				USHARE_DEBUG("move childs...\n");
				*ptr = *(ptr + 1);
				ptr++;
			}
			
			ut->root_entry->child_count--;
			USHARE_DEBUG("realloc...\n");
			ut->root_entry->childs = 
				(struct upnp_entry_t **)realloc (ut->root_entry->childs, 
												 (ut->root_entry->child_count + 1) * 
												 sizeof (*(ut->root_entry->childs)));
			if (ut->root_entry->childs == NULL)
			{
				log_error("realloc error");
				return FALSE;
			}
			ut->root_entry->childs[ut->root_entry->child_count] = NULL;
			
			USHARE_DEBUG("found %s in meta tree. free end...\n", fullpath);
			break;
		}
	}
	
	return TRUE;
}

/* 
 * fn		int build_content_list(ushare_t *ut)
 * brief	�������ô���content_list.	
 *
 * param[in/out]	ut		ushareȫ������ָ��.
 *
 * return			BOOL		����ֵ
 * retval			TRUE		�ɹ�
 *					FALSE		ʧ��
 *
 * note	written by  18Dec11, LI CHENGLONG	
 */
BOOL build_content_list(ushare_t *ut)
{
	struct list_head *pos = NULL;
	USHARE_FOLDER_INFO *pEntry = NULL;
	
	assert(ut != NULL);

	if (ut == NULL)
	{
		return FALSE;
	}
	
	USHARE_DEBUG("build_content_list BEGIN...\n");
	list_for_each(pos, &ut->initInfo.folderList)
	{
		pEntry = list_entry(pos, USHARE_FOLDER_INFO, list);
		USHARE_DEBUG("add pEntry->path=%s, pEntry->dispName=%s\n", pEntry->path, pEntry->dispName);
		ut->contentlist = ushare_content_add(ut->contentlist, 
											 pEntry->path, 
											 pEntry->dispName, 
											 NO_SYNC_MTREE);
	/*now ut->contentlist maybe null,do not use ut->contentlist->count,
	  Added by LI CHENGLONG , 2012-Jan-05.*/
		
	}
	USHARE_DEBUG("build_content_list END...\n");

  	if (!ut->contentlist)
	{	
		USHARE_DEBUG("ut->conententlist=null\n");
		ut->contentlist = (content_list*) malloc (sizeof(content_list));
		ut->contentlist->content = NULL;
		ut->contentlist->displayName = NULL;
		ut->contentlist->count = 0;
	}

	return TRUE;
}

/* 
 * fn		content_list *content_priv_del(content_list *list, const char *item)
 * brief	��list������ɾ��·����Ϊitem�Ĺ���Ŀ¼.	
 * details	content.c�в�δʵ��ͨ��·����ɾ������Ŀ¼�ķ���,����ͨ����װ�ṩһ����������Ľӿ�.	
 *
 * param[in]	list		content����
 * param[in]	item		·���ַ���	
 *
 * return		content_list*	
 * retval		���ظ��º������ͷ.
 *
 * note	written by  18Dec11, LI CHENGLONG	
 */
content_list *content_priv_del(content_list *list, const char *item)
/* 
 * brief: Added by LI CHENGLONG, 2011-Dec-18.
 *		  1.find the entry.
 *		  2.call content_del in content.c
 */
{
	int cursor = 0;
	
	assert((item != NULL));

	if ((item == NULL))
	{
		return list;
	}

	USHARE_DEBUG("delte content %s in contentlist begin...\n", item);
	while (cursor < list->count)
	{
		/*�Ƚ�·������ͬ, Added by LI CHENGLONG , 2011-Dec-18.*/
		if (strcmp(item, list->content[cursor]) == 0)
		{
			USHARE_DEBUG("delte content %s in contentlist...\n", item);
			return content_del(list, cursor);			
		}
		cursor++;
	}

	return list;
}

/* 
 * fn 		content_list *ushare_content_add(content_list *list, 
 											 const char *item, 
 											 const char *name, 
 											 char flag)
 * brief	����item��name���һ������·��.	
 * details	��/var/usbdiskĿ¼��ɨ�����д��ڵĹ���·��,��ӵ�����������.	
 *
 * param[in]	list			Ŀ¼��������.
 * param[in]	item			����Ŀ¼��·��.
 * param[in]	name			����Ŀ¼����.
 * param[in]	flag			�Ƿ�Ҫ����ý����Ϣ.
 *
 * return		content_list*
 * retval		���ظ��º������ͷ.
 *
 * note	written by  16Dec11, LI CHENGLONG	
 */
content_list *ushare_content_add(content_list *list, const char *item, const char *name, char flag)
{
	int suffix = 0;
	int cursor = 0;
	struct stat st;
	int dirCount = 0;
	char *fullPath = NULL;
	char newName[BUFLEN_32] = {0};
	struct dirent	**nameList = NULL;
	
	/*list may be NULL so we don't care. Added by LI CHENGLONG , 2011-Dec-25.*/
	assert((item != NULL) && (name != NULL));

	if ((item == NULL) || (name == NULL))
	{
		return list;
	}

	if (list == NULL)
	{
		USHARE_DEBUG("add to a new created list\n");
	}
	
	/* 
	 * brief: Added by LI CHENGLONG, 2011-Dec-18.
	 *		  item �� name�Ĵ���.
	 */
	dirCount = scandir(USB_DISK_DIRECTORY, &nameList, 0, alphasort);
	if (dirCount <= 0)
	{
		return list;
	}
	
	USHARE_DEBUG("after scandir %s, dirCount=%d\n", USB_DISK_DIRECTORY, dirCount);
	
	for (cursor = 0, suffix = 0; cursor < dirCount; cursor++)
	{
		if (nameList[cursor]->d_name[0] == '.')
		{
			free(nameList[cursor]);
			continue;
		}
		
		/*���Ե��������䴴��������. Added by LI CHENGLONG , 2012-Jan-05.*/
		if (strcmp(nameList[cursor]->d_name, USBVM) == 0)
		{
			free(nameList[cursor]);
			continue;
		}
		
		fullPath = (char *)	malloc(strlen(USB_DISK_DIRECTORY) + 
								   strlen(nameList[cursor]->d_name) +
								   strlen(item) + 2);

		if (fullPath == NULL)
		{
			log_error("malloc error");
			/* Add by chz to avoid memory leaks, 2012-10-30 */
			while (cursor < dirCount)
			{
				free(nameList[cursor++]);
			}
			free(nameList);
			/* end add */
			return list;
		}
		
		sprintf (fullPath, "%s/%s%s", USB_DISK_DIRECTORY, nameList[cursor]->d_name, item);
		
		if (stat (fullPath, &st) < 0)
		{
			free(nameList[cursor]);
			free(fullPath);
			continue;
		}
		
		/*����������, Added by LI CHENGLONG , 2012-Jan-05.*/
		if (S_ISDIR (st.st_mode) && !(S_ISLNK(st.st_mode)))
		{
			
			if (suffix == 0)
			{
				list = content_add(list, fullPath, name);			
			}
			else
			{
				/*������ڶ��ͬ����·���� ����ĸ��׺������, Added by LI CHENGLONG , 2011-Dec-18.*/
				snprintf(newName, BUFLEN_32, "%s_%c", name, 'A' + suffix - 1);
				list = content_add(list, fullPath, newName);
			}
			
			/* 
			 * brief: Added by LI CHENGLONG, 2011-Dec-18.
			 *		  1.call to add upnp_entry_t .
			 		  2.call to add redblack tree node.
			 */
			if (flag == SYNC_MTREE)
			{
				/*�������һ��, Added by LI CHENGLONG , 2011-Dec-18.*/
				ushare_entry_add(list, list->count - 1);
			}
			
			suffix++;
		}
		
	    free (nameList[cursor]);
    	free (fullPath);
	}

	/* Add by chz to avoid memory leaks, 2012-10-25 */
	free(nameList);
	/* end add */

	return list;
}

/* 
 * fn		content_list *ushare_content_del(content_list *list, 
 											 const char *item, 
 											 const char *name, 
 											 char flag)
 * brief	����item��nameɾ��һ������·��.	
 * details	��/var/usbdiskĿ¼��ɨ�����д��ڵĹ���·��,�ӹ���������ɾ������·��.
 *
 * param[in]	list			Ŀ¼��������.
 * param[in]	item			����Ŀ¼��·��.
 * param[in]	name			����Ŀ¼����.
 * param[in]	flag			�Ƿ�Ҫ����ý����Ϣ.
 *
 * return		content_list*
 * retval		���ظ��º������ͷ.
 *
 * note	written by  16Dec11, LI CHENGLONG	
 */
content_list *ushare_content_del(content_list *list, const char *item, const char *name, char flag)
/* 
 * brief: Added by LI CHENGLONG, 2011-Dec-18.
 *		  1.scan /var/usbdisk
 		  2.call content_priv_del
 		  3.sync? 
 */
{
	int cursor = 0;
	int dirCount = 0;
	char *fullPath = NULL;
	struct dirent	**nameList = NULL;

	assert((item != NULL) && (name != NULL));

	if (list == NULL)
	{
		return list;
	}

	if ((item == NULL) || (name == NULL))
	{
		return list;
	}

	dirCount = scandir(USB_DISK_DIRECTORY, &nameList, 0, alphasort); 
	if (dirCount <= 0)
	{
		return list;
	}

	USHARE_DEBUG("after scandir dirCount=%d\n", dirCount);

	for (cursor = 0; cursor < dirCount; cursor++)
	{
		if (nameList[cursor]->d_name[0] == '.')
		{
			free(nameList[cursor]);
			continue;
		}

		fullPath = (char *)	malloc(strlen(USB_DISK_DIRECTORY) + 
								   strlen(nameList[cursor]->d_name) +
								   strlen(item) + 2);

		if (fullPath == NULL)
		{
			log_error("malloc error");
			return list;
		}

		sprintf (fullPath, "%s/%s%s", USB_DISK_DIRECTORY, nameList[cursor]->d_name, item);
		/* 
		 * brief: Added by LI CHENGLONG, 2011-Dec-18.
		 *		  1.call to delete upnp_entry_t .
		 		  2.call to delete redblack tree node.
		 */
		 

		/* SYNC_MTREE */
		if (flag == SYNC_MTREE)
		{
			USHARE_DEBUG("ushare delete entry %s with sync...\n", fullPath);
			ushare_entry_del(fullPath);
		}
		
		list = content_priv_del(list, fullPath);

	    free (nameList[cursor]);
    	free (fullPath);
	}

	/* Add by chz to avoid memory leaks, 2012-10-25 */
	free(nameList);
	/* end add */

	return list;
}

/* 
 * fn		BOOL ushareStartAutoScanWorker()
 * brief	Ushare ��������ʱ������woker. ���ڶ�ʱ.	
 *
 * return	BOOL		����ֵ
 * retval	FALSE		ʧ��
 *			TRUE		�ɹ�
 *
 * note	written by  25Nov11, LI CHENGLONG	
 */
BOOL ushareStartAutoScanWorker()
{
	int ret = 0;
	ThreadPoolJob job;
	
	TPJobInit( &job, ( start_routine ) ushareAutoScanWorker, NULL);
	TPJobSetFreeFunction( &job, ( free_routine )NULL);
	TPJobSetPriority( &job, MED_PRIORITY );
		
	ret = TimerThreadSchedule(&gTimerThread,
							  1,
							  REL_SEC,
							  &job,
							  SHORT_TERM,
							  NULL);
	
	if (ret != 0)
	{
		return FALSE;
	}

	return TRUE;
}

/* 
 *--------------------------------------------------------------------------------------------------
 *�������뿪���ر���ص�һЩ����ʵ��.
 *--------------------------------------------------------------------------------------------------
 */

/* 
 * fn	 	BOOL ushareSwitchOn() 
 * brief	����media server����.
 *
 * return	BOOL		����ֵ
 * retval	TRUE		�����ɹ�	
 *			FALSE		����ʧ��
 *
 * note	written by  24Nov11, LI CHENGLONG	
 */
BOOL ushareSwitchOn()
{
	int len = 0;
	int res = 0;
	char modelDesc[100];
	char presentUrl[64];
	char ipa[16] = {0};
	
	if (ut->ip)
	{
		/*store old ip, Added by LI CHENGLONG , 2012-May-25.*/
		snprintf(ipa, sizeof(ipa), "%s", ut->ip);

		free(ut->ip);
		ut->ip = NULL;
		ut->ip = get_iface_address (ut->interface); 
	}
	else
	{
		memset(ipa, 0, sizeof(ipa));
		ut->ip = get_iface_address (ut->interface);
	}

	/*new interface not up, Added by LI CHENGLONG , 2012-May-25.*/
	if (!ut->ip)
	{
		ushare_free (ut);
		return FALSE;
	}
	
	/*ip����,���³�ʼ��upnp��, Added by LI CHENGLONG , 2012-May-25.*/
	if(strncmp(ipa, ut->ip, 16))
	{
		UpnpUnRegisterRootDevice (ut->dev);
		UpnpFinish ();
		ushareInitUPnPEnv();
	}


	snprintf(modelDesc, sizeof(modelDesc), 
			 "%s %s: DLNA Media Server", 
			 ut->initInfo.manuInfo.modelName, 
			 ut->initInfo.manuInfo.devModelVersion);

	snprintf(presentUrl, sizeof(presentUrl), "http://%s", ut->ip);
	
#ifdef HAVE_DLNA
	  if (ut->dlna_enabled)
	  {
		len = 0;
		ut->description =
		  dlna_dms_description_get (ut->initInfo.serverName,
									ut->initInfo.manuInfo.manufacturer,
									ut->initInfo.manuInfo.devManufacturerURL,
									modelDesc,
									ut->model_name,
									"1.0",
									ut->initInfo.manuInfo.devManufacturerURL,
									"USHARE-01",
									ut->udn,
									presentUrl,
									"/web/cms.xml",
									"/web/cms_control",
									"/web/cms_event",
									"/web/cds.xml",
									"/web/cds_control",
									"/web/cds_event");
		if (!ut->description)
		  return FALSE;
	  }
	  else
	  {
#endif /* HAVE_DLNA */ 
	  len = strlen (UPNP_DESCRIPTION) + strlen (ut->initInfo.serverName)
		+ strlen (ut->model_name) + strlen (ut->udn) + 1;
	  ut->description = (char *) malloc (len * sizeof (char));
	  memset (ut->description, 0, len);
	  sprintf (ut->description, UPNP_DESCRIPTION, ut->initInfo.serverName, ut->model_name, ut->udn);
#ifdef HAVE_DLNA
	  }
#endif /* HAVE_DLNA */

  res = UpnpRegisterRootDevice2 (UPNPREG_BUF_DESC, ut->description, 0, 1,
                                 device_callback_event_handler,
                                 NULL, &(ut->dev));
  if (res != UPNP_E_SUCCESS)
  {
    USHARE_DEBUG("Cannot register UPnP device\n");
    free (ut->description);
    return FALSE;
  }

  res = UpnpUnRegisterRootDevice (ut->dev);
  if (res != UPNP_E_SUCCESS)
  {
    log_error ("Cannot unregister UPnP device\n");
    free (ut->description);
    return FALSE;
  }

  res = UpnpRegisterRootDevice2 (UPNPREG_BUF_DESC, ut->description, 0, 1,
                                 device_callback_event_handler,
                                 NULL, &(ut->dev));
  if (res != UPNP_E_SUCCESS)
  {
    log_error ("Cannot register UPnP device\n");
    free (ut->description);
    return FALSE;
  }

  USHARE_DEBUG ("Sending UPnP advertisement for device ...\n");
  UpnpSendAdvertisement (ut->dev, 1800);

  USHARE_DEBUG ("Listening for control point connections ...\n");

  if (ut->description)
    free (ut->description);

  return TRUE;	
}

/* 
 * fn	 	BOOL ushareSwitchOff() 
 * brief	�ر�media server����.	
 *
 * return	BOOL	����ֵ
 * retval	TRUE	�رճɹ�
 *			FALSE 	�ر�ʧ��	
 *
 * note	written by  24Nov11, LI CHENGLONG	
 */
BOOL ushareSwitchOff()
{	
	if (UPNP_E_SUCCESS != UpnpUnRegisterRootDevice (ut->dev))
	{
		USHARE_DEBUG("ushare:debug:switch off new folder %d, %s .\n", __LINE__, __FUNCTION__);	
		return FALSE;
	}

	USHARE_DEBUG("ushare switch off.......\n");
	return TRUE;
}

/* 
 * fn		BOOL ushareSwitchOnOff(BOOL onOff)
 * brief	������ر�MediaServer����.	
 *
 * param[in]	onOff		������ر�MediaServer����.
 *
 * return		BOOL		����ֵ
 * retval		TRUE		���óɹ�
 *				FALSE		����ʧ��
 *
 * note	written by  24Nov11, LI CHENGLONG	
 */
BOOL ushareSwitchOnOff(BOOL onOff)
{
	if (onOff == TRUE)
	{
		return ushareSwitchOn();
	}
	else if (onOff == FALSE)
	{
		return ushareSwitchOff();
	}

	return FALSE;
}

/* 
 * fn		BOOL ushareParseConfig(int argc, char **argv) 
 * brief	ushare���̽�������,����������, �����ļ�, ��ʼ����Ϣ.	
 * details	������һ������,���е����ö�������ut�ṹ��,�����������о͸���ut������.	
 *
 * param[in]	argc		ֱ����main�����Ĳ�������
 * param[in]	argv		ֱ����main�����Ĳ�������	
 *
 * return		BOOL		����ֵ
 * retval		TRUE		�����ɹ�
 *				FALSE		���������г��ִ���
 *
 * note	written by  25Nov11, LI CHENGLONG	
 */
BOOL ushareParseConfig(int argc, char **argv)
{
	setup_i18n ();
	setup_iconv ();
	/* Parse args before cfg file, as we may override the default file */
	if (parse_command_line (ut, argc, argv) < 0)
	{
	  ushare_free (ut);
	  return FALSE;
	}
	if (parse_config_file (ut) < 0)
	{
	  /* fprintf here, because syslog not yet ready */
	  fprintf (stderr, "Warning: can't parse file \"%s\".\n",
			   ut->cfg_file ? ut->cfg_file : SYSCONFDIR "/" USHARE_CONFIG_FILE);
	}
	
	if (ut->contentlist)
	{
		free(ut->contentlist);
		ut->contentlist = NULL;
	}
	
	build_content_list(ut);

	if (!ut->contentlist)
	{	
		USHARE_DEBUG("ut->conententlist=null\n");
		ut->contentlist = (content_list*) malloc (sizeof(content_list));
		ut->contentlist->content = NULL;
		ut->contentlist->displayName = NULL;
		ut->contentlist->count = 0;
	}	

	if (ut->xbox360)
	{
		char *name;

		name = malloc (strlen (XBOX_MODEL_NAME) + strlen (ut->model_name) + 4);
		sprintf (name, "%s (%s)", XBOX_MODEL_NAME, ut->model_name);
		free (ut->model_name);
		ut->model_name = strdup (name);
		free (name);

		ut->starting_id = STARTING_ENTRY_ID_XBOX360;
	}
	if (ut->daemon)
	{
		/* starting syslog feature as soon as possible */
		start_log ();
	}	

	if (!has_iface (ut->interface))
	{
		ushare_free (ut);
		return FALSE;
	}

	ut->udn = create_udn (ut->interface);
	if (!ut->udn)
	{
		ushare_free (ut);
		return FALSE;
	}

	ut->ip = get_iface_address (ut->interface);	
	if (!ut->ip)
	{
		ushare_free (ut);
		return EXIT_FAILURE;
	}

	if (ut->daemon)
	{
		int err;
		err = daemon (0, 0);
		if (err == -1)
		{
			log_error ("Error: failed to daemonize program : %s\n", strerror (err));
			ushare_free (ut);
			return FALSE;
		}
	}
	else
	{
		display_headers ();
	}	

	signal (SIGINT, UPnPBreak);
	signal (SIGHUP, reload_config);	
	
#ifdef INCLUDE_TELNET
	  if (ut->use_telnet)
	  {
		if (ctrl_telnet_start (ut->telnet_port) < 0)
		{
		  ushare_free (ut);
		  return FALSE;
		}
		
		ctrl_telnet_register ("kill", ushare_kill, "Terminates the uShare server");
	  }
#endif /*INCLUDE_TELNET*/

	return TRUE;
}

/* 
 * fn		BOOL ushareInitUPnPEnv() 
 * brief	ushare���̳�ʼ��upnp�Ļ���,����upnpinit���õ�,����ushare����������ֻ��ʼ��һ��.	
 *
 * return		BOOL	����ֵ
 * retval		TRUE	��ʼ���ɹ�
 *				FALSE	��ʼ��ʧ��
 *
 * note	written by  25Nov11, LI CHENGLONG	
 */
BOOL ushareInitUPnPEnv()
{
	int res;
	ThreadPoolAttr newAttr;

	if (!ut || !ut->udn || !ut->ip)
	{
		if (ut->udn)
		{
			USHARE_DEBUG("ut->udn=%s\n", ut->udn);
		}
		if (ut->ip)
		{
			USHARE_DEBUG("ut->ip=%s", ut->ip);
		}
		return FALSE;
	}

	/*set threadpool attr here!*/
	newAttr.minThreads = 2;
	#if defined(INCLUDE_VOIP)&&defined(SUPPORT_SDRAM_32M)
	newAttr.maxThreads = 3;
	#else
	newAttr.maxThreads = 5;
	#endif
	newAttr.maxIdleTime = 5000;
	newAttr.maxJobsTotal = 100;
	newAttr.jobsPerThread = 1;
	newAttr.starvationTime = 1000;
	newAttr.schedPolicy = 0;
	if(uppn_setMPoolAttr(&newAttr) != UPNP_E_SUCCESS)
	{
		log_error("can't set miniserverpool attr in ushare\n");
	}

	USHARE_DEBUG("Initializing UPnP subsystem ...\n");
	res = UpnpInit (ut->ip, ut->port);
	if (res != UPNP_E_SUCCESS)
	{
		log_error ("Cannot initialize UPnP subsystem\n");
		return FALSE;
	}

	if (UpnpSetMaxContentLength (UPNP_MAX_CONTENT_LENGTH) != UPNP_E_SUCCESS)
		USHARE_DEBUG ("Could not set Max content UPnP\n");

	if (ut->xbox360)
		USHARE_DEBUG ("Starting in XboX 360 compliant profile ...\n");

#ifdef HAVE_DLNA
	if (ut->dlna_enabled)
	{
		USHARE_DEBUG ("Starting in DLNA compliant profile ...\n");
		ut->dlna = dlna_init ();
		dlna_set_verbosity (ut->dlna, ut->verbose ? 1 : 0);
		/*ʵ�������ﲻӦ�����ü���׺���������п��ܵ��²�����ȷ�����޺�׺������׺�����ļ�. 
		Added by LI CHENGLONG , 2011-Dec-07.*/
		dlna_set_extension_check (ut->dlna, 1);
		/*����ע�������ܹ��������ļ�����, Added by LI CHENGLONG , 2011-Dec-07.*/
		dlna_register_all_media_profiles (ut->dlna);
	}
#endif /* HAVE_DLNA */

	ut->port = UpnpGetServerPort();
	USHARE_DEBUG ("UPnP MediaServer listening on %s:%d\n",
	UpnpGetServerIpAddress (), ut->port);

	UpnpEnableWebserver (TRUE);

	res = UpnpSetVirtualDirCallbacks (&virtual_dir_callbacks);
	if (res != UPNP_E_SUCCESS)
	{
		USHARE_DEBUG ("Cannot set virtual directory callbacks\n");
		return FALSE;
	}

	res = UpnpAddVirtualDir (VIRTUAL_DIR);
	if (res != UPNP_E_SUCCESS)
	{
		USHARE_DEBUG ("Cannot add virtual directory for web server\n");
		return FALSE;
	}	

	return TRUE;
}

/* 
 * fn		int daemonize(int nochdir, int noclose)
 * brief	����ǰ���̷ŵ���̨����.	
 *
 * param[in]	nochdir		�Ƿ�ı��̨���̵Ĺ���Ŀ¼
 * param[in]	noclode		�Ƿ�ı��̨���̵ı�׼�������	
 *
 * return		int		������
 * retval		-1		ʧ��
 *				0		�ɹ�
 *
 * note	written by  24Nov11, LI CHENGLONG	
 */
int daemonize(int nochdir, int noclose)
{
	int fd;
	
	switch (fork()) 
	{
	/*fork error*/	
	case -1:		
		return (-1);
	/*child*/	

	case 0:
		break;
		
	/*parents*/	
	default:
		exit(0);
	}
	
	/*run the program in new session*/
	if (setsid() == -1)
	{
		return (-1);
	}

	/*change dir or not*/
	if (!nochdir)
	{
		chdir("/");
	}

	/*append stard description file to /dev/null,except stard out*/
	if (!noclose && (fd = open("/dev/null", O_RDWR, 0)) != -1) 
	{
		(void)dup2(fd, 0); //input
		//(void)dup2(fd, 1); //output
		//(void)dup2(fd, 2); //error
		if (fd > 2)
		{
			(void)close (fd);		
		}
	}
	
	return (0);	
}
/**************************************************************************************************/
/*                                           GLOBAL_FUNCTIONS                                     */
/**************************************************************************************************/

