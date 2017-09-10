/*
 * metadata.c : GeeXboX uShare CDS Metadata DB.
 * Originally developped for the GeeXboX project.
 * Copyright (C) 2005-2007 Benjamin Zores <ben@geexbox.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>

#include <upnp.h>
#include <upnptools.h>

#include "mime.h"
#include "metadata.h"
#include "util_iconv.h"
#include "content.h"
#include "gettext.h"
#include "trace.h"

extern struct ushare_t *ut;

#define TITLE_UNKNOWN "unknown"

#define USHARE_MAX_MEDIA_FILES_NUM 1000

#define MAX_URL_SIZE 32

struct upnp_entry_lookup_t {
  int id;
  struct upnp_entry_t *entry_ptr;
};

static char *
getExtension (const char *filename)
{
  char *str = NULL;

  str = strrchr (filename, '.');
  if (str)
    str++;

  return str;
}

static struct mime_type_t *
getMimeType (const char *extension)
{
  extern struct mime_type_t MIME_Type_List[];
  struct mime_type_t *list;

  if (!extension)
    return NULL;

  list = MIME_Type_List;
  while (list->extension)
  {
    if (!strcasecmp (list->extension, extension))
      return list;
    list++;
  }

  return NULL;
}

static bool
is_valid_extension (const char *extension)
{
  if (!extension)
    return false;

  if (getMimeType (extension))
    return true;

  return false;
}

/*port from netgear,(used when scandir), Added by LI CHENGLONG , 2011-Nov-13.*/

/* 
 * fn		int ends_with(const char * haystack, const char * needle)
 * brief	判断字符串是否是以needle指向的字符串结尾.		
 *
 * param[in]	haystack		源字符串
 * param[in]	needle			要查找的字符串
 *
 * return	int	
 * retval	0		字符串haystack 不是以needle结尾.	
 *
 * note	written by  13Nov11, LI CHENGLONG	
 */
int
ends_with(const char * haystack, const char * needle)
{
	const char * end;
	int nlen = strlen(needle);
	int hlen = strlen(haystack);

	if( nlen > hlen )
		return 0;
 	end = haystack + hlen - nlen;

	return (strcasecmp(end, needle) ? 0 : 1);
}


/* 
 * fn		int is_video(const char * file)
 * brief	判断一个文件是否是视频文件.
 * details	当增加视频格式时需要更新该函数.	
 *
 * param[in]	file	文件名
 *
 * return		int
 * retval		0		不是视频文件
 *				1		是视频文件
 *
 * note	written by  13Nov11, LI CHENGLONG	
 */
int
is_video(const char * file)
{
	return (ends_with(file, ".mpg") ||
		 	ends_with(file, ".mpeg")||
			ends_with(file, ".avi") ||
			ends_with(file, ".divx")||
			ends_with(file, ".asf") ||
			ends_with(file, ".wmv") ||
			ends_with(file, ".m4v") ||
			ends_with(file, ".mkv") ||
			ends_with(file, ".vob") ||
			ends_with(file, ".ts")  ||
			ends_with(file, ".mov") ||
			ends_with(file, ".dv")  ||
			ends_with(file, ".mjpg")||
			ends_with(file, ".mjpeg")  ||
			ends_with(file, ".mpe")  ||
			ends_with(file, ".mp2p")  ||
			ends_with(file, ".m1v")  ||
			ends_with(file, ".m2v")  ||
			ends_with(file, ".m4p")  ||
			ends_with(file, ".mp4") ||
			ends_with(file, ".mp4ps")  ||
			ends_with(file, ".ogm")  ||
			ends_with(file, ".rmvb")  ||
			ends_with(file, ".mov")  ||
			ends_with(file, ".qt")  ||
			ends_with(file, ".hdmov"));
	
}

/* 
 * fn		int is_audio(const char * file)
 * brief	判断一个文件是否是音频文件.	
 * details	当增加音频格式时需要更新该函数.	
 *
 * param[in]	file	文件名
 *
 * return	int	
 * retval	0			不是音频文件
 *			1			是音频文件
 *
 * note	written by  13Nov11, LI CHENGLONG	
 */
int
is_audio(const char * file)
{
	return (ends_with(file, ".mp3") ||
			ends_with(file, ".flac")||
			ends_with(file, ".wma") || 
			ends_with(file, ".aac") ||
			ends_with(file, ".wav") || 
			ends_with(file, ".ogg") ||
			ends_with(file, ".pcm") || 
			ends_with(file, ".ac3") ||
			ends_with(file, ".at3p")||
			ends_with(file, ".lpcm")||
			ends_with(file, ".aif") ||
			ends_with(file, ".aiff") ||
			ends_with(file, ".au") ||
			ends_with(file, ".m4a") ||
			ends_with(file, ".amr") ||
			ends_with(file, ".acm") ||
			ends_with(file, ".snd") ||
			ends_with(file, ".dts") ||
			ends_with(file, ".rmi") ||
			ends_with(file, ".aif") ||
			ends_with(file, ".mp1") ||
			ends_with(file, ".mp2") ||
			ends_with(file, ".mpa") ||
			ends_with(file, ".l16") ||
			ends_with(file, ".mka") ||
			ends_with(file, ".ra") ||
			ends_with(file, ".rm") ||
			ends_with(file, ".ram"));
}

/* 
 * fn		int is_image(const char * file)
 * brief	判断一个文件是否是图像文件.	
 * details	当增加音频格式时需要更新该函数.	
 *
 * param[in]	file	文件名
 *
 * return	int	
 * retval	0			不是图像文件
 *			1			是图像文件
 *
 * note	written by  13Nov11, LI CHENGLONG	
 */
int
is_image(const char * file)
{
	return (ends_with(file, ".jpg") || 
			ends_with(file, ".jpeg")||
			ends_with(file, ".jpe")||
			ends_with(file, ".png")||
			ends_with(file, ".bmp")||
			ends_with(file, ".ico")||
			ends_with(file, ".gif")||
			ends_with(file, ".pcd")||
			ends_with(file, ".pnm")||
			ends_with(file, ".ppm")||
			ends_with(file, ".qti")||
			ends_with(file, ".qtf")||
			ends_with(file, ".qtif")||
			ends_with(file, ".tif")||
			ends_with(file, ".tiff"));
}

/* 
 * fn		int is_playlist(const char * file)
 * brief	判断一个文件是否是playlist文件.	
 * details	当增加playlist格式时需要更新该函数.	
 *
 * param[in]	file	文件名
 *
 * return	int	
 * retval	0			不是playlist文件
 *			1			是playlist文件
 *
 * note	written by  13Nov11, LI CHENGLONG	
 */
 int
is_playlist(const char * file)
{
	return (ends_with(file, ".m3u") || ends_with(file, ".pls"));
}


/* 
 * fn		int filter_media(const struct dirent *d)
 * brief	扫描目录时的过滤器,只对感兴趣的媒体文件和目录进行收集.	
 *
 * param[in]	dirent		dir entry 目录项
 *
 * return		int
 * retval		0			不符合需求,被过滤掉
 *				1			符合需求,收集起来
 *
 * note	written by  13Nov11, LI CHENGLONG	
 */
int
filter_media(const struct dirent *d)
{
	return ( (*d->d_name != '.') &&
	         ((d->d_type == DT_DIR) ||
	          (d->d_type == DT_LNK) ||
	          (d->d_type == DT_UNKNOWN) ||
	          ((d->d_type == DT_REG) &&
	           (is_image(d->d_name) ||
	            is_audio(d->d_name) ||
	            is_video(d->d_name)
	           )
	       ) ));
}
/* Ended by LI CHENGLONG , 2011-Nov-13.*/


static int
get_list_length (void *list)
{
  void **l = list;
  int n = 0;

  while (*(l++))
    n++;

  return n;
}

static xml_convert_t xml_convert[] = {
  {'"' , "&quot;"},
  {'&' , "&amp;"},
  {'\'', "&apos;"},
  {'<' , "&lt;"},
  {'>' , "&gt;"},
  {'\n', "&#xA;"},
  {'\r', "&#xD;"},
  {'\t', "&#x9;"},
  {0, NULL},
};

static char *
get_xmlconvert (int c)
{
  int j;
  for (j = 0; xml_convert[j].xml; j++)
  {
    if (c == xml_convert[j].charac)
      return xml_convert[j].xml;
  }
  return NULL;
}

static char *
convert_xml (const char *title)
{
  char *newtitle, *s, *t, *xml;
  int nbconvert = 0;

  /* calculate extra size needed */
  for (t = (char*) title; *t; t++)
  {
    xml = get_xmlconvert (*t);
    if (xml)
      nbconvert += strlen (xml) - 1;
  }
  if (!nbconvert)
    return NULL;

  newtitle = s = (char*) malloc (strlen (title) + nbconvert + 1);

  for (t = (char*) title; *t; t++)
  {
    xml = get_xmlconvert (*t);
    if (xml)
    {
      strcpy (s, xml);
      s += strlen (xml);
    }
    else
      *s++ = *t;
  }
  *s = '\0';

  return newtitle;
}

static struct mime_type_t Container_MIME_Type =
  { NULL, "object.container.storageFolder", NULL};

struct upnp_entry_t *
upnp_entry_new (struct ushare_t *ut, const char *name, const char *fullpath,
                struct upnp_entry_t *parent, off_t size, int dir)
{
  struct upnp_entry_t *entry = NULL;
  char *title = NULL, *x = NULL;
  char url_tmp[MAX_URL_SIZE] = { '\0' };
  char *title_or_name = NULL;

  if (!name)
    return NULL;

  entry = (struct upnp_entry_t *) malloc (sizeof (struct upnp_entry_t));

#ifdef HAVE_DLNA
  entry->dlna_profile = NULL;
  entry->url = NULL;
  if (ut->dlna_enabled && fullpath && !dir)
  {
#if 0	/*use simple guess.*/
    dlna_profile_t *p = dlna_guess_media_profile (ut->dlna, fullpath);
#endif /* 0 */

	/* special. by HouXB, 08Oct10 */
	dlna_profile_t *p = dlna_guess_media_profile2 (ut->dlna, fullpath);
	/* printf("use new DLNA profile\n"); */
	
	//printf("dlna profile guess.\n");
	
    if (!p)
    {
      free (entry);
      return NULL;
    }
    entry->dlna_profile = p;
  }
#endif /* HAVE_DLNA */
 
  if (ut->xbox360)
  {
    if (ut->root_entry)
      entry->id = ut->starting_id + ut->nr_entries++;
    else
      entry->id = 0; /* Creating the root node so don't use the usual IDs */
  }
  else
    entry->id = ut->starting_id + ut->nr_entries++;
  
  entry->fullpath = fullpath ? strdup (fullpath) : NULL;
  entry->parent = parent;
  entry->child_count =  dir ? 0 : -1;
  entry->title = NULL;

  entry->childs = (struct upnp_entry_t **)
    malloc (sizeof (struct upnp_entry_t *));
  *(entry->childs) = NULL;

  if (!dir) /* item */
    {
#ifdef HAVE_DLNA
      if (ut->dlna_enabled)
        entry->mime_type = NULL;
      else
      {
#endif /* HAVE_DLNA */
      struct mime_type_t *mime = getMimeType (getExtension (name));
      if (!mime)
      {
        --ut->nr_entries; 
        upnp_entry_free (ut, entry);
        log_error ("Invalid Mime type for %s, entry ignored", name);
        return NULL;
      }
      entry->mime_type = mime;
#ifdef HAVE_DLNA
      }
#endif /* HAVE_DLNA */
      
      if (snprintf (url_tmp, MAX_URL_SIZE, "%d.%s",
                    entry->id, getExtension (name)) >= MAX_URL_SIZE)
        log_error ("URL string too long for id %d, truncated!!", entry->id);

      /* Only malloc() what we really need */
      entry->url = strdup (url_tmp);
    }
  else /* container */
    {
      entry->mime_type = &Container_MIME_Type;
      entry->url = NULL;
    }

  /* Try Iconv'ing the name but if it fails the end device
     may still be able to handle it */
  title = iconv_convert (name);
  if (title)
    title_or_name = title;
  else
  {
    if (ut->override_iconv_err)
    {
      title_or_name = strdup (name);
      log_error ("Entry invalid name id=%d [%s]\n", entry->id, name);
    }
    else
    {
      upnp_entry_free (ut, entry);
      log_error ("Freeing entry invalid name id=%d [%s]\n", entry->id, name);
      return NULL;
    }
  }

  if (!dir)
  {
    x = strrchr (title_or_name, '.');
    if (x)  /* avoid displaying file extension */
      *x = '\0';
  }
  x = convert_xml (title_or_name);
  if (x)
  {
    free (title_or_name);
    title_or_name = x;
  }
  entry->title = title_or_name;

  if (!strcmp (title_or_name, "")) /* DIDL dc:title can't be empty */
  {
    free (title_or_name);
    entry->title = strdup (TITLE_UNKNOWN);
  }

  entry->size = size;
  entry->fd = -1;
  /* Add by chz, to sync without rebuilding all. 2012-10-23 */
  entry->exist_flag = FALSE;
  /* end add */

  if (entry->id && entry->url)
    log_verbose ("Entry->URL (%d): %s\n", entry->id, entry->url);
	
  return entry;
}

/* Seperate recursive free() function in order to avoid freeing off
 * the parents child list within the freeing of the first child, as
 * the only entry which is not part of a childs list is the root entry
 */
static void
_upnp_entry_free (struct upnp_entry_t *entry)
{
  struct upnp_entry_t **childs;
  struct upnp_entry_lookup_t entry_lookup;
  struct upnp_entry_lookup_t *lk;

  if (!entry)
    return;

	if (entry->mime_type != &Container_MIME_Type)
	{
		if (ut->mediaFilesNum > 0)
		{
			ut->mediaFilesNum--;
		}
	}
  
  /* 
   * brief: Added by LI CHENGLONG, 2011-Dec-18.
   *		redblack node need to be freeed at the same time.
   */
 	/*释放掉entry对应的rbnode,对应的id不存在了, Added by LI CHENGLONG , 2011-Dec-18.*/
	entry_lookup.id = entry->id;
	lk = rbdelete(&entry_lookup, ut->rb);
	/* Add by chz. 2012-10-30 
	 * We have to free the upnp_entry_lookup_t pointer at the same time. */
	if (lk)
	{
		free(lk);
	}
	else
	{
		log_error("There's no upnp_entry_lookup_t struct with id: %d.\n", entry_lookup.id);
	}
	/* end add by chz */
 	/* Ended by LI CHENGLONG , 2011-Dec-18.*/

  if (entry->fullpath)
  {
    free (entry->fullpath);
  }
  if (entry->title)
    free (entry->title);
  if (entry->url)
    free (entry->url);
#ifdef HAVE_DLNA
  if (entry->dlna_profile)
    entry->dlna_profile = NULL;
#endif /* HAVE_DLNA */

  for (childs = entry->childs; *childs; childs++)
    _upnp_entry_free (*childs);
  free (entry->childs);
  /* Add by chz to avoid memory leaks. 2012-11-01 */
  free (entry);
  /* end add */
}

void
upnp_entry_free (struct ushare_t *ut, struct upnp_entry_t *entry)
{
  if (!ut || !entry)
    return;

  /* Free all entries (i.e. children) */
  if (entry == ut->root_entry)
  {
    struct upnp_entry_t *entry_found = NULL;
    struct upnp_entry_lookup_t *lk = NULL;
    RBLIST *rblist;
    int i = 0;

    rblist = rbopenlist (ut->rb);
    lk = (struct upnp_entry_lookup_t *) rbreadlist (rblist);

    while (lk)
    {
      entry_found = lk->entry_ptr;
      if (entry_found)
      {
	 	if (entry_found->fullpath)
	 	  free (entry_found->fullpath);
	 	if (entry_found->title)
	 	  free (entry_found->title);
	 	if (entry_found->url)
	 	  free (entry_found->url);

		/* Add by chz to avoid memory leaks. 2012-11-01 */
		free(entry_found->childs);
		/* end add */

		free (entry_found);
	 	i++;
      }

      free (lk); /* delete the lookup */
      lk = (struct upnp_entry_lookup_t *) rbreadlist (rblist);
    }

    rbcloselist (rblist);
    rbdestroy (ut->rb);
    ut->rb = NULL;

	/* Add by chz to avoid memory leaks, free the root entry. 2012-11-01 */
	if (entry->fullpath)
 	  free(entry->fullpath);
 	if (entry->title)
 	  free(entry->title);
 	if (entry->url)
 	  free(entry->url);

	free(entry->childs);
	free(entry);
	/* end add */
	
	ut->mediaFilesNum = 0;
    log_verbose ("Freed [%d] entries\n", i);
  }
  else
    _upnp_entry_free (entry);

  /* Delete by chz. we must free this pointer in _upnp_entry_free() recursively. 2012-11-01 */
  //free (entry);
  /* end delete */
}

/* 
 * fn		void upnp_entry_del(struct upnp_entry_t *entry)
 * brief	Delete the upnp node
 *
 * param        entry
 *
 * return		void
 *
 * note:    	written by Cheng HuaZhuo	
 */
void
upnp_entry_del(struct upnp_entry_t *entry)
{
	struct upnp_entry_t *parent;
	struct upnp_entry_t **child;
	struct upnp_entry_t **ptr;
	
	if (entry == NULL)
	{
		log_error("Get NULL pointer in upnp_entry_del()!\n");
		return;
	}

	if (entry == ut->root_entry)
	{
		upnp_entry_free(ut, entry);
		return;
	}

	parent = entry->parent;
	
	for (child = parent->childs; *child; child++)
	{
		if (*child == entry)
		{
			upnp_entry_free(ut, entry);

			ptr = child;
			while(*ptr)
			{
				*ptr = *(ptr + 1);
				ptr++;
			}
			
			parent->child_count--;

			parent->childs = (struct upnp_entry_t **)realloc(parent->childs, 
								(parent->child_count + 1) * sizeof(*(parent->childs)));
			if (parent->childs == NULL)
			{
				log_error("realloc error upnp_entry_del()");
				return FALSE;
			}
			parent->childs[parent->child_count] = NULL;
			
			break;
		}
	}
}


void
upnp_entry_add_child (struct ushare_t *ut,
                      struct upnp_entry_t *entry, struct upnp_entry_t *child)
{
  struct upnp_entry_lookup_t *entry_lookup_ptr = NULL;
  struct upnp_entry_t **childs;
  int n;

  if (!entry || !child)
    return;

  for (childs = entry->childs; *childs; childs++)
    if (*childs == child)
      return;

  n = get_list_length ((void *) entry->childs) + 1;
  entry->childs = (struct upnp_entry_t **)
    realloc (entry->childs, (n + 1) * sizeof (*(entry->childs)));
  entry->childs[n] = NULL;
  entry->childs[n - 1] = child;
  entry->child_count++;
  
  entry_lookup_ptr = (struct upnp_entry_lookup_t *)
    malloc (sizeof (struct upnp_entry_lookup_t));
  entry_lookup_ptr->id = child->id;
  entry_lookup_ptr->entry_ptr = child;

  if (rbsearch ((void *) entry_lookup_ptr, ut->rb) == NULL)
    log_info (_("Failed to add the RB lookup tree\n"));
}

/*根据id查找upnp_entry_t* Added by LI CHENGLONG , 2011-Dec-08.*/
struct upnp_entry_t *
upnp_get_entry (struct ushare_t *ut, int id)
{
  struct upnp_entry_lookup_t *res, entry_lookup;

  log_verbose ("Looking for entry id %d\n", id);
  if (id == 0) /* We do not store the root (id 0) as it is not a child */
    return ut->root_entry;

  entry_lookup.id = id;
  res = (struct upnp_entry_lookup_t *)
    rbfind ((void *) &entry_lookup, ut->rb);

  if (res)
  {
    log_verbose ("Found at %p\n",
                 ((struct upnp_entry_lookup_t *) res)->entry_ptr);
    return ((struct upnp_entry_lookup_t *) res)->entry_ptr;
  }

  log_verbose ("Not Found\n");

  return NULL;
}

static void
metadata_add_file (struct ushare_t *ut, struct upnp_entry_t *entry,
                   const char *file, const char *name, struct stat *st_ptr)
{
  if (!entry || !file || !name)
    return;

#ifdef HAVE_DLNA
  if (ut->dlna_enabled || is_valid_extension (getExtension (file)))
#else
  if (is_valid_extension (getExtension (file)))
#endif
  {
    struct upnp_entry_t *child = NULL;
	
    child = upnp_entry_new (ut, name, file, entry, st_ptr->st_size, false);
    if (child)
      upnp_entry_add_child (ut, entry, child);

	/* Add by chz. 2012-10-23*/
	ut->mediaFilesNum++;
	/* end add */
  }
	/* Move forward, we should only record valid media files. 2012-10-23*/
	//ut->mediaFilesNum++;
	/* end modify */
}


/* 
 * fn		struct upnp_entry_t *upnp_is_child_exist(struct upnp_entry_t *parent, char *fullPath)
 * brief	Is the parent has a child with this full path?
 *
 * param        parent, title
 *
 * return		pointer of child if exist, or NULL if not exist
 *
 * note:    	written by Cheng HuaZhuo	
 */
struct upnp_entry_t *
upnp_is_child_exist(struct upnp_entry_t *parent, char *fullPath)
{
	int i;

	for (i=0; i < parent->child_count; i++)
	{
		if (strcmp(fullPath, parent->childs[i]->fullpath) == 0)
		{
			return parent->childs[i];
		}
	}

	return NULL;
}

/* 
 * fn		void metadata_sync_container(struct ushare_t *ut,
 *                       				 struct upnp_entry_t *entry, 
 *										 const char *container)
 * brief	Sync all the media files
 *
 * param    ut, entry, container
 *
 * return	void
 *
 * note:    written by Cheng HuaZhuo	
 */
void
metadata_sync_container(struct ushare_t *ut,
                        struct upnp_entry_t *entry, 
                        const char *fullPath)
{
	struct dirent **namelist = NULL;
	int n,i;
	struct upnp_entry_t *child = NULL;
	int child_num = 0;

	if (!entry || !fullPath)
	{
		log_error("metadata_sync_container() get NULL pointer!\n");
		return;
	}

	n = scandir(fullPath, &namelist, filter_media, alphasort);
	if (n < 0)
	{
		perror ("scandir");
		return;
	}

	for (i=0; i < n; i++)
	{
		struct stat st;
		char *childFullPath = NULL;

		if (namelist[i]->d_name[0] == '.')
		{
			free(namelist[i]);
			continue;
		}

		childFullPath = (char *)malloc(strlen(fullPath) + strlen(namelist[i]->d_name) + 2);
		sprintf(childFullPath, "%s/%s", fullPath, namelist[i]->d_name);

		if (stat(childFullPath, &st) < 0)
		{
			free(namelist[i]);
			free(childFullPath);
			continue;
		}

		child = upnp_is_child_exist(entry, childFullPath);

		if (S_ISDIR (st.st_mode))
		{
	        if (!child)
	        {
				child = upnp_entry_new(ut, namelist[i]->d_name, childFullPath, entry, 0, true);
	        	if (child)
	        	{
	        		upnp_entry_add_child(ut, entry, child);
	        	}
				else
				{
					log_error("Fail to create upnp node for %s!\n", namelist[i]->d_name);
					free(namelist[i]);
					free(childFullPath);
					continue;
				}
	        }

			child->exist_flag = TRUE;

			metadata_sync_container(ut, child, childFullPath);
		}
		else
		{
			if (!child)
			{	
#ifdef HAVE_DLNA
	  		 	if (ut->dlna_enabled || is_valid_extension(getExtension(childFullPath)))
#else
	  		  	if (is_valid_extension(getExtension(childFullPath)))
#endif
	  		 	{
					if (ut->mediaFilesNum >= USHARE_MAX_MEDIA_FILES_NUM)
					{
						log_error("ushare have reached max media files limit\n");
						free(namelist[i]);
						free(childFullPath);
						continue;
					}
					
		  		    child = upnp_entry_new(ut, namelist[i]->d_name, childFullPath, entry, 0, false);
		  		    if (child)
		  		    {
		  		    	upnp_entry_add_child(ut, entry, child);
						
						ut->mediaFilesNum++;
		  		    }
					else
					{
						log_error("Fail to create upnp node for %s!\n", namelist[i]->d_name);
						free(namelist[i]);
						free(childFullPath);
						continue;
					}
	  		 	}
			}

			child->exist_flag = TRUE;
		}

		free(namelist[i]);
		free(childFullPath);
	}
	
	free(namelist);

	for (i=0; i < entry->child_count; i++)
	{
		/* If exist_flag is true, means it's exist in the disk right now.
		 * just set it to false for next time scaning. If it's false, means 
		 * we should delete this node.*/
		if (entry->childs[i]->exist_flag)
		{
			entry->childs[i]->exist_flag = false;
		}
		else
		{
			upnp_entry_del(entry->childs[i]);
			/* After deleting the node, the entry->child_count subtracts 1,
			 * and the childs pointer array was shortened. */
			i--;
		}
	}
}

void
metadata_add_container (struct ushare_t *ut,
                        struct upnp_entry_t *entry, const char *container)
{
  struct dirent **namelist;
  int n,i;

  if (!entry || !container)
    return;

  /* Delete by chz to avoid memory leaks, 2012-10-26 */
  /*
  if (ut->mediaFilesNum >= USHARE_MAX_MEDIA_FILES_NUM)
  {
  		log_error("ushare have reached max media files limit\n");
  		return;
  }
  */
  /* end delete */

  /*只将媒体文件存储,其他的文件不分配内存,不关心,若添加媒体则需要更新filter_media函数,
   *Added by LI CHENGLONG , 2011-Nov-13.*/
  n = scandir (container, &namelist, filter_media, alphasort);
  if (n < 0)
  {
    perror ("scandir");
    return;
  }

  for (i = 0; i < n; i++)
  {
    struct stat st;
    char *fullpath = NULL;

    if (namelist[i]->d_name[0] == '.')
    {
      free (namelist[i]);
      continue;
    }

	/* Delete by chz to avoid memory leaks, 2012-10-26 */
	/*
	if (ut->mediaFilesNum >= USHARE_MAX_MEDIA_FILES_NUM)
	{
		log_error("ushare have reached max media files limit\n");
		//return;
	}
    */
	/* end delete */

    fullpath = (char *)
      malloc (strlen (container) + strlen (namelist[i]->d_name) + 2);
    sprintf (fullpath, "%s/%s", container, namelist[i]->d_name);

    log_verbose ("%s\n", fullpath);

    if (stat (fullpath, &st) < 0)
    {
      free (namelist[i]);
      free (fullpath);
      continue;
    }

    if (S_ISDIR (st.st_mode))
    {
      struct upnp_entry_t *child = NULL;

      child = upnp_entry_new (ut, namelist[i]->d_name,
                              fullpath, entry, 0, true);
      if (child)
      {
        metadata_add_container (ut, child, fullpath);
        upnp_entry_add_child (ut, entry, child);
      }
    }
    else
    {
	  /* Add by chz to avoid memory leaks, 2012-10-26 */
	  if (ut->mediaFilesNum >= USHARE_MAX_MEDIA_FILES_NUM)
	  {
	  	log_error("ushare have reached max media files limit\n");
	  	free(namelist[i]);
    	free(fullpath);
		continue;
	  }
	  /* end add */
	  
      metadata_add_file (ut, entry, fullpath, namelist[i]->d_name, &st);
    }
    free (namelist[i]);
    free (fullpath);
  }
  free (namelist);
}

void
free_metadata_list (struct ushare_t *ut)
{

  USHARE_DEBUG("free all metatata list...\n");

  ut->init = 0;
  if (ut->root_entry)
    upnp_entry_free (ut, ut->root_entry);
  ut->root_entry = NULL;
  ut->nr_entries = 0;

  if (ut->rb)
  {
    rbdestroy (ut->rb);
    ut->rb = NULL;
  }

  ut->rb = rbinit (rb_compare, NULL);
  if (!ut->rb)
    log_error (_("Cannot create RB tree for lookups\n"));

  ut->mediaFilesNum = 0;
}

void
build_metadata_list (struct ushare_t *ut)
{
  int i;
	static int rebuildIndex = 0;

	rebuildIndex++;
	if (rebuildIndex > STARTING_ENTRY_ID_CYCLE)
	{
		rebuildIndex = 0;
	}


	if (ut->xbox360)
	{
  		ut->starting_id = STARTING_ENTRY_ID_XBOX360 + rebuildIndex * STARTING_ENTRY_ID_STEP;
	}
	else
	{
		ut->starting_id = STARTING_ENTRY_ID_DEFAULT + rebuildIndex * STARTING_ENTRY_ID_STEP;
	}

  
	if (!ut->contentlist)
	{	
		USHARE_DEBUG("ut->conententlist=null\n");
		ut->contentlist = (content_list*) malloc (sizeof(content_list));
		ut->contentlist->content = NULL;
		ut->contentlist->displayName = NULL;
		ut->contentlist->count = 0;
	}

  /* build root entry */
  if (!ut->root_entry)
    ut->root_entry = upnp_entry_new (ut, "root", NULL, NULL, -1, true);

  /* add files from content directory */
  for (i=0 ; i < ut->contentlist->count ; i++)
  {
    struct upnp_entry_t *entry = NULL;
    char *title = NULL;
    int size = 0;

    log_info (_("Looking for files in content directory : %s\n"),
              ut->contentlist->content[i]);

    size = strlen (ut->contentlist->content[i]);
    if (ut->contentlist->content[i][size - 1] == '/')
      ut->contentlist->content[i][size - 1] = '\0';


	if (strlen(ut->contentlist->displayName[i]) > 0)
	{
		title = ut->contentlist->displayName[i];
	}	
	else
	{
	    title = strrchr (ut->contentlist->content[i], '/');
	    if (title)
	      title++;
	    else
	    {
	      /* directly use content directory name if no '/' before basename */
	      title = ut->contentlist->content[i];
	    }
	}

    entry = upnp_entry_new (ut, title, ut->contentlist->content[i],
                            ut->root_entry, -1, true);

    if (!entry)
      continue;
    upnp_entry_add_child (ut, ut->root_entry, entry);
    metadata_add_container (ut, entry, ut->contentlist->content[i]);
  }

  log_info (_("Found %d files and subdirectories.\n"), ut->nr_entries);
  ut->init = 1;
}


/* 
 * fn		void sync_metadata_list(struct ushare_t * ut)
 * brief	Sync all the media files
 *
 * param        ut
 *
 * return		void
 *
 * note:    	written by Cheng HuaZhuo	
 */
void
sync_metadata_list(struct ushare_t * ut)
{
    int i;
	struct upnp_entry_t *child = NULL;
	char *title = NULL;
	int size = 0;
  
	if ((!ut->contentlist) || (!ut->root_entry))
	{	
		log_error(_("Get NULL pointer in sync_metadata_list()!\n"));
		return;
	}

	/* sync files in content directory */
	for (i=0 ; i < ut->contentlist->count ; i++)
	{
		log_info(_("Looking for files to sync in content directory : %s\n"),
		          ut->contentlist->content[i]);

		size = strlen(ut->contentlist->content[i]);
		if (ut->contentlist->content[i][size - 1] == '/')
		{
		  ut->contentlist->content[i][size - 1] = '\0';
		}

		if (strlen(ut->contentlist->displayName[i]) > 0)
		{
			title = ut->contentlist->displayName[i];
		}
		else
		{
			log_error(_("The content %d has a NULL displayName!\n"), i);
			continue;
		}

		child = upnp_is_child_exist(ut->root_entry, ut->contentlist->content[i]);
		if (!child)
		{
			child = upnp_entry_new(ut, title, ut->contentlist->content[i],
                            ut->root_entry, -1, true);
		    if (!child)
		    {
				log_error(_("Fail to make upnp node for content %d!\n"), i);
		    	continue;
		    }
			
		    upnp_entry_add_child(ut, ut->root_entry, child);
		}

		child->exist_flag = TRUE;

		metadata_sync_container(ut, child, ut->contentlist->content[i]);
	}

	for (i=0; i < ut->root_entry->child_count; i++)
	{
		/* If exist_flag is true, means it's exist in the disk right now.
		 * just set it to false for next time scaning. If it's false, means 
		 * we should delete this node.*/
		if (ut->root_entry->childs[i]->exist_flag)
		{
			ut->root_entry->childs[i]->exist_flag = false;
		}
		else
		{
			upnp_entry_del(ut->root_entry->childs[i]);
			
			i--;
		}
	}

	log_info(_("Sync %d media files.\n"), ut->mediaFilesNum);
}


int
rb_compare (const void *pa, const void *pb,
            const void *config __attribute__ ((unused)))
{
  struct upnp_entry_lookup_t *a, *b;

  a = (struct upnp_entry_lookup_t *) pa;
  b = (struct upnp_entry_lookup_t *) pb;

  if (a->id < b->id)
    return -1;

  if (a->id > b->id)
    return 1;

  return 0;
}

