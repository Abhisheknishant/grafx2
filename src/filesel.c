/* vim:expandtab:ts=2 sw=2:
*/
/*  Grafx2 - The Ultimate 256-color bitmap paint program

    Copyright 2018 Thomas Bernard
    Copyright 2014 Sergii Pylypenko
    Copyright 2011 Pawel Góralski
    Copyright 2009 Franck Charlet
    Copyright 2008 Peter Gordon
    Copyright 2008 Yves Rizoud
    Copyright 2007 Adrien Destugues
    Copyright 1996-2001 Sunset Design (Guillaume Dorme & Karl Maritaud)

    Grafx2 is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; version 2
    of the License.

    Grafx2 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Grafx2; if not, see <http://www.gnu.org/licenses/>
*/

#if defined(__amigaos4__) || defined(__AROS__) || defined(__MORPHOS__) || defined(__amigaos__)
    #include <proto/dos.h>
    #include <sys/types.h>
#elif defined (__MINT__)
    #include <mint/sysbind.h>
#elif defined(WIN32)
    #include <windows.h>
    #include <commdlg.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#ifdef _MSC_VER
#include <stdio.h>
#define strdup _strdup
#if _MSC_VER < 1900
#define snprintf _snprintf
#endif
#else
#include <strings.h>
#include <unistd.h>
#endif
#include <ctype.h>
#include <sys/types.h>

#include "const.h"
#include "struct.h"
#include "global.h"
#include "misc.h"
#include "errors.h"
#include "io.h"
#include "windows.h"
#include "screen.h"
#include "loadsave.h"
#include "mountlist.h"
#include "engine.h"
#include "readline.h"
#include "input.h"
#include "help.h"
#include "unicode.h"
#include "filesel.h"

#define NORMAL_FILE_COLOR    MC_Light // color du texte pour une ligne de
  // fichier non sélectionné
#define NORMAL_DIRECTORY_COLOR MC_Dark // color du texte pour une ligne de
  // répertoire non sélectionné
#define NORMAL_BACKGROUND_COLOR       MC_Black  // color du fond  pour une ligne
  // non sélectionnée
#define SELECTED_FILE_COLOR    MC_White // color du texte pour une ligne de
  // fichier sélectionnée
#define SELECTED_DIRECTORY_COLOR MC_Light // color du texte pour une ligne de
  // repértoire sélectionnée
#define SELECTED_BACKGROUND_COLOR       MC_Dark // color du fond  pour une ligne
  // sélectionnée

// -- Native fileselector for WIN32

// Returns 0 if all ok, something else if failed
byte Native_filesel(byte load)
{
  //load = load;
#if WIN32
  OPENFILENAME ofn;
  char szFileName[MAX_PATH] = "";

  ZeroMemory(&ofn, sizeof(ofn));

  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = GFX2_Get_Window_Handle();
  ofn.lpstrFilter = TEXT("Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0");
#ifdef UNICODE
#else
  ofn.lpstrFile = szFileName;
#endif
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_EXPLORER;
  if(load) ofn.Flags |= OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
  ofn.lpstrDefExt = TEXT("txt");

  if(load)
  {
    if (GetOpenFileName(&ofn))
      // Do something usefull with the filename stored in szFileName 
      return 0;
    else
      // error - check if its just user pressing cancel or something else
      return CommDlgExtendedError();
  } else if(GetSaveFileName(&ofn)) {
    return 0;
  } else {
    // Check if cancel
    return CommDlgExtendedError();
  }
#else
  (void)load; // unused
#if 0
  /* if the native fileselector is used, we should implement it where needed
   * OS X ? GTK ? etc. */
  #ifndef __linux__ // This makes no sense on X11-oriented platforms. Nothing is really native there.
    #warning "EXPERIMENTAL function for native fileselector not available for this platform!"
  #endif
#endif
    return 255; // fail !
#endif
}  

// -- "Standard" fileselector for other platforms

// -- Fileselector data

static T_Fileselector Filelist;

/// Selector settings to use, for all functions called by Load_or_save
static T_Selector_settings * Selector;

/// Name of the current directory
//static char Selector_directory[1024];
/// Filename (without directory) of the highlighted file
static char Selector_filename[256];
static word Selector_filename_unicode[256];

// Conventions:
//
// * Le fileselect modifie le répertoire courant. Ceci permet de n'avoir
//   qu'un findfirst dans le répertoire courant à faire:

/**
 * Update T_Fileselector::Nb_files T_Fileselector::Nb_directories T_Fileselector::Nb_elements
 * counts.
 * Also update the list index T_Fileselector::Index
 * @param list the linked list to update
 */
static void Recount_files(T_Fileselector *list)
{
  T_Fileselector_item *item;

  list->Nb_files=0;
  list->Nb_directories=0;
  list->Nb_elements=0;
  
  for (item = list->First; item != NULL; item = item->Next)
  {
    if (item->Type == 0)
      list->Nb_files ++;
    else
      list->Nb_directories ++;
    list->Nb_elements ++;
  }
  
  if (list->Index)
  {
    free(list->Index);
    list->Index = NULL;
  }
  
  if (list->Nb_elements>0)
  {
    int i;
    
    list->Index = (T_Fileselector_item **) malloc(list->Nb_elements * sizeof(T_Fileselector_item *));
    if (list->Index)
    {
      // Fill the index
      for (item = list->First, i=0; item != NULL; item = item->Next, i++)
      {
        list->Index[i] = item;
      }
    }
    // If the malloc failed, we're probably in trouble, but I don't know
    // how to recover from that..I'll just make the index bulletproof.
  }
}

/**
 * This function free all item in the list, but not the list itself.
 * @param list the linked list
 */
void Free_fileselector_list(T_Fileselector *list)
{
  // Pointeur temporaire de destruction
  T_Fileselector_item * temp_item;

  while (list->First!=NULL)
  {
    // On mémorise l'adresse du premier élément de la liste
    temp_item =list->First;
    // On fait avancer la tête de la liste
    list->First=list->First->Next;
    // Et on efface l'ancien premier élément de la liste
    free(temp_item->Unicode_full_name);
    free(temp_item->Unicode_short_name);
    free(temp_item);
    temp_item = NULL;
  }
  Recount_files(list);
}

word * Format_filename_unicode(const word * fname, word max_length, int type)
{
  static word result[40];
  int         c;
  int         other_cursor;
  int         pos_last_dot;

 // safety
  if (max_length>40)
    max_length=40;

  if (Unicode_char_strcmp(fname,PARENT_DIR)==0)
  {
    Unicode_char_strlcpy(result, "\x11 PARENT DIRECTORY", 40);
    // Append spaces
    for (c=18; c<max_length-1; c++)
      result[c]=' ';
    result[c]='\0';
  }
  else if (fname[0]=='.' || type==1 || type==2)
  {
    // Files ".something" or drives or directories: Aligned left on (max_length-1) chars max
    // Initialize as all spaces
    for (c=0; c<max_length-1; c++)
      result[c]=' ';
    result[c]='\0';

    for (c=0;fname[c]!='\0' && c < max_length-1;c++)
      result[c]=fname[c];
    // A special character indicates the filename is truncated
    if (c >= max_length-1)
      result[max_length-2]=(byte)ELLIPSIS_CHARACTER;
  }
  else
  {
    // Initialize as all spaces
    for (c = 0; c<max_length-1; c++)
      result[c]=' ';
    result[c]='\0';

    result[max_length-5]='.';

    // Look for the last dot in filename
    pos_last_dot = Position_last_dot_unicode(fname);

    // Copy the part before the dot
    for (c=0; c!=pos_last_dot && fname[c]!='\0'; c++)
    {
      if (c > max_length-6)
      {
        result[max_length-6]=(byte)ELLIPSIS_CHARACTER;
        break;
      }
      result[c]=fname[c];
    }

    // Ensuite on recopie la partie qui suit le point (si nécessaire):
    if (pos_last_dot != -1)
    {
      for (c = pos_last_dot+1,other_cursor=max_length-4;fname[c]!='\0' && other_cursor < max_length-1;c++,other_cursor++)
        result[other_cursor]=fname[c];
    }
  }
  return result;
}

char * Format_filename(const char * fname, word max_length, int type)
{
  static char result[40];
  int         c;
  int         other_cursor;
  int         pos_last_dot;
#ifdef ENABLE_FILENAMES_ICONV
  /* convert file name from UTF8 to ANSI */
  char        converted_fname[MAX_PATH_CHARACTERS];
  {
    char * input = (char *)fname;
    size_t inbytesleft = strlen(fname);
    char * output = converted_fname;
    size_t outbytesleft = sizeof(converted_fname)-1;
    if(cd != (iconv_t)-1 && (ssize_t)iconv(cd, &input, &inbytesleft, &output, &outbytesleft) >= 0)
    {
      *output = '\0';
      fname = converted_fname;
    }
  }
#endif /* ENABLE_FILENAMES_ICONV */

  // safety
  if (max_length>40)
    max_length=40;
  
  if (strcmp(fname,PARENT_DIR)==0)
  {
    strcpy(result,"\x11 PARENT DIRECTORY");
    // Append spaces
    for (c=18; c<max_length-1; c++)
      result[c]=' ';
    result[c]='\0';
  }
  else if (fname[0]=='.' || type==1 || type==2)
  {
    // Files ".something" or drives or directories: Aligned left on (max_length-1) chars max
    // Initialize as all spaces
    for (c=0; c<max_length-1; c++)
      result[c]=' ';
    result[c]='\0';
    
    for (c=0;fname[c]!='\0' && c < max_length-1;c++)
      result[c]=fname[c];
    // A special character indicates the filename is truncated
    if (c >= max_length-1)
      result[max_length-2]=ELLIPSIS_CHARACTER;
  }
  else
  {
    // Initialize as all spaces
    for (c = 0; c<max_length-1; c++)
      result[c]=' ';
    result[c]='\0';
       
    result[max_length-5]='.';
    
    // Look for the last dot in filename
    pos_last_dot = Position_last_dot(fname);

    // Copy the part before the dot
    for (c=0; c!=pos_last_dot && fname[c]!='\0'; c++)
    {
      if (c > max_length-6)
      {
        result[max_length-6]=ELLIPSIS_CHARACTER;
        break;
      }
      result[c]=fname[c];
    }

    // Ensuite on recopie la partie qui suit le point (si nécessaire):
    if (pos_last_dot != -1)
    {
      for (c = pos_last_dot+1,other_cursor=max_length-4;fname[c]!='\0' && other_cursor < max_length-1;c++,other_cursor++)
        result[other_cursor]=fname[c];
    }
  }
  return result;
}


/**
 * Add an item to the file selector linked list
 * @param list the linked list
 * @param full_name the file name
 * @param short_name the file name truncated to display in the file selector
 * @param type the type of the item : 0 = File, 1 = Directory, 2 = Drive
 * @param icon the icon for the item
 * @return a pointer to the newly added item
 * @return NULL in case of error
 */
T_Fileselector_item * Add_element_to_list(T_Fileselector *list, const char * full_name, const char *short_name, int type, enum ICON_TYPES icon)
{
  // Working element
  T_Fileselector_item * temp_item;
  size_t full_name_len, short_name_len;

  full_name_len = strlen(full_name) + 1;
  short_name_len = strlen(short_name) + 1;
  // Allocate enough room for one struct + the visible label
  temp_item=(T_Fileselector_item *)malloc(sizeof(T_Fileselector_item)+full_name_len);
  if (temp_item == NULL)  // memory allocation error
    return NULL;
  memset(temp_item, 0, sizeof(T_Fileselector_item));

  if (short_name_len > sizeof(temp_item->Short_name))
    short_name_len = sizeof(temp_item->Short_name) - 1; // without terminating 0
  // Initialize element
  memcpy(temp_item->Short_name,short_name,short_name_len);
  memcpy(temp_item->Full_name,full_name,full_name_len);
  temp_item->Type = type;
  temp_item->Icon = icon;

  // Doubly-linked
  temp_item->Next    =list->First;
  temp_item->Previous=NULL;

  if (list->First!=NULL)
    list->First->Previous=temp_item;
    
  // Put new element at the beginning
  list->First=temp_item;
  return temp_item;
}

///
/// Checks if a file has the requested file extension.
/// The extension string can end with a ';' (remainder is ignored).
/// This function allows wildcard '?', and '*' if it's the only character.
int Check_extension(const char *filename_ext, const char * filter)
{
  int c;
  
  if (filter[0] == '*')
    return 1;

  // filename without extension
  if (filename_ext == NULL || filename_ext[0] == '\0')
    return (filter[0] == '\0' || filter[0] == ';');

  // Vérification caractère par caractère, case-insensitive.
  for (c = 0; !(filter[c] == '\0' || filter[c] == ';'); c++)
  {
    if (filter[c] != '?' &&
      tolower(filter[c]) != tolower(filename_ext[c]))
      return 0;
  }
  return filename_ext[c] == '\0';
}


// -- Lecture d'une liste de fichiers ---------------------------------------
struct Read_dir_pdata
{
  T_Fileselector *list;
  const char * filter;
};

static void Read_dir_callback(void * pdata, const char *file_name, const word *unicode_name, byte is_file, byte is_directory, byte is_hidden)
{
  T_Fileselector_item * item;
  struct Read_dir_pdata * p = (struct Read_dir_pdata *)pdata;

  if (p == NULL) // error !
    return;

  // Ignore 'current directory' entry
  if ( !strcmp(file_name, "."))
    return;

  // entries tagged "directory" :
  if (is_directory)
  {
    // On Windows, the presence of a "parent directory" entry has proven
    // unreliable on non-physical drives :
    // Sometimes it's missing, sometimes it's present even at root...
    // We skip it here and add a specific check after the loop
#if defined(WIN32)
    if (!strcmp(file_name, PARENT_DIR))
      return;
#endif

    // Don't display hidden file, unless requested by options
    if (!Config.Show_hidden_directories && is_hidden)
      return;

    // Add to list
    item = Add_element_to_list(p->list, file_name, Format_filename(file_name, 19, 1), 1, ICON_NONE);
    if (item != NULL && unicode_name != NULL)
    {
      item->Unicode_full_name = Unicode_strdup(unicode_name);
      item->Unicode_short_name = Unicode_strdup(Format_filename_unicode(unicode_name, 19, 1));
    }
    p->list->Nb_directories++;
  }
  else if (is_file && // It's a file
          (Config.Show_hidden_files || !is_hidden))
  {
    const char * ext = p->filter;
    const char * file_name_ext = NULL;
#ifdef WIN32
	char long_ext[16];
#endif
	int pos_last_dot = Position_last_dot(file_name);

	if (pos_last_dot >= 0)
      file_name_ext = file_name + pos_last_dot + 1;
#ifdef WIN32
    if (unicode_name && unicode_name[0] != 0)
    {
      pos_last_dot = Position_last_dot_unicode(unicode_name);
      if (pos_last_dot >= 0)
      {
        int i;
        pos_last_dot++;
        for (i = 0; i < (int)sizeof(long_ext) - 1; i++)
          long_ext[i] = (unicode_name[pos_last_dot + i] < 256) ? unicode_name[pos_last_dot + i] : '?';
        long_ext[i] = '\0';
        file_name_ext = long_ext;
      }
    }
#endif
    while (ext!=NULL)
    {
      if (Check_extension(file_name_ext, ext))
      {
        // Add to list
        item = Add_element_to_list(p->list, file_name, Format_filename(file_name, 19, 0), 0, ICON_NONE);
        if (item != NULL && unicode_name != NULL)
        {
          item->Unicode_full_name = Unicode_strdup(unicode_name);
          item->Unicode_short_name = Unicode_strdup(Format_filename_unicode(unicode_name, 19, 0));
        }
        p->list->Nb_files++;
        // Stop searching
        break;
      }
      else
      {
        ext = strchr(ext, ';');
        if (ext)
          ext++;
      }
    }
  }
}



void Read_list_of_files(T_Fileselector *list, byte selected_format)
//  Cette procédure charge dans la liste chainée les fichiers dont l'extension
// correspond au format demandé.
{
  struct Read_dir_pdata callback_data;
  const char * current_path = NULL;
  char curdir[MAX_PATH_CHARACTERS];
#if defined (__MINT__)
  T_Fileselector_item *item=0;
  bool bFound=false;
#endif

  callback_data.list = list;
  // Tout d'abord, on déduit du format demandé un filtre à utiliser:
  callback_data.filter = Get_fileformat(selected_format)->Extensions;

  // Ensuite, on vide la liste actuelle:
  Free_fileselector_list(list);
  // Après effacement, il ne reste ni fichier ni répertoire dans la liste

  // On lit tous les répertoires:
  current_path = Get_current_directory(curdir, NULL, MAX_PATH_CHARACTERS);

  For_each_directory_entry(current_path, &callback_data, Read_dir_callback);
  
  // Now here's OS-specific code to determine if "parent directory" entry
  // should appear.

#if defined(__MORPHOS__) || defined(__AROS__) || defined (__amigaos4__) || defined(__amigaos__)
  // Amiga systems: always
  Add_element_to_list(list, PARENT_DIR, Format_filename(PARENT_DIR,19,1), 1, ICON_NONE);
  list->Nb_directories ++;
  
#elif defined (WIN32)
  // Windows :
  if (((current_path[0]>='a'&&current_path[0]<='z')||(current_path[0]>='A'&&current_path[0]<='Z')) &&
    current_path[1]==':' &&
    (
      (current_path[2]=='\0') ||
      (current_path[2]=='/'&&current_path[3]=='\0') ||
      (current_path[2]=='\\'&&current_path[3]=='\0')
    ))
  {
    // Path is X:\ or X:/ or X:
    // so don't display parent directory
  }
  else
  {
    Add_element_to_list(list, PARENT_DIR, Format_filename(PARENT_DIR,19,1), 1, ICON_NONE);
    list->Nb_directories ++;
  }
  
#elif defined (__MINT__)
  // check if ".." exists if not add it
  // FreeMinT lists ".." already, but this is not so for TOS 
  // simply adding it will cause double PARENT_DIR under FreeMiNT
  
  bFound= false;
  
  for (item = list->First; (((item != NULL) && (bFound==false))); item = item->Next){
    if (item->Type == 1){
      if(strncmp(item->Full_name,PARENT_DIR,(sizeof(char)*2))==0) bFound=true;
    }
  }
  
  if(!bFound){
    Add_element_to_list(list,PARENT_DIR,Format_filename(PARENT_DIR,19,1),1,ICON_NONE); // add if not present
    list->Nb_directories ++;  
  }
  
#endif

  current_path = NULL;

  if (list->Nb_files==0 && list->Nb_directories==0)
  {
    // This can happen on some empty network drives.
    // Add a dummy entry because the fileselector doesn't
    // seem to support empty list.
    Add_element_to_list(list, ".",Format_filename(".",19,1),1,ICON_NONE);
  }

  Recount_files(list);
}

#if defined(__amigaos4__) || defined(__AROS__) || defined(__MORPHOS__) || defined(__amigaos__)
void bstrtostr( BSTR in, STRPTR out, TEXT max )
{
  STRPTR iptr;
  dword i;
  dword len;

#if defined(__AROS__)
  iptr = AROS_BSTR_ADDR( in );
  len = AROS_BSTR_strlen( in );
#else
  iptr = BADDR( in );
  len = iptr[0];
  iptr++;
#endif

  if( max > len ) max = len;

  for( i=0; i<max; i++ , iptr++ ) out[i] = *iptr;

  out[i] = 0;
}
#endif

// -- Lecture d'une liste de lecteurs / volumes -----------------------------
void Read_list_of_drives(T_Fileselector *list, byte name_length)
{
#if defined(__MINT__)
    char drive_name[]="A:\\";
    unsigned long drive_bits=0;
    int drive_index=0;
    int bit_index=0;
#endif
  
  // Empty the current content of fileselector:
  Free_fileselector_list(list);
  // Reset number of items
  list->Nb_files=0;
  list->Nb_directories=0;

  #if defined(__amigaos4__) || defined(__AROS__) || defined(__MORPHOS__) || defined(__amigaos__)
  {
    struct DosList *dl;
    char tmp[256];

    dl = LockDosList( LDF_VOLUMES | LDF_ASSIGNS | LDF_READ );
    if( dl )
    {
      while( ( dl = NextDosEntry( dl, LDF_VOLUMES | LDF_ASSIGNS | LDF_READ ) ) )
      {
        bstrtostr( dl->dol_Name, tmp, 254 );
        strcat( tmp, ":" );
        Add_element_to_list(list, tmp, Format_filename(tmp, name_length, 2), 2, ICON_NONE );
        list->Nb_directories++;
      }
      UnLockDosList( LDF_VOLUMES | LDF_ASSIGNS | LDF_READ );
    }
  }
  #elif defined (WIN32)
  {
    char drive_name[]="A:\\";
    int drive_bits = GetLogicalDrives();
    int drive_index;
    int bit_index;
    byte icon;
    
    // Sous Windows, on a la totale, presque aussi bien que sous DOS:
    drive_index = 0;
    for (bit_index=0; bit_index<26 && drive_index<23; bit_index++)
    {
      if ( (1 << bit_index) & drive_bits )
      {
        // On a ce lecteur, il faut maintenant déterminer son type "physique".
        // pour profiter des jolies icones de X-man.
        char drive_path[]="A:\\";
        // Cette API Windows est étrange, je dois m'y faire...
        drive_path[0]='A'+bit_index;
        switch (GetDriveTypeA(drive_path))
        {
          case DRIVE_CDROM:
            icon=ICON_CDROM;
            break;
          case DRIVE_REMOTE:
            icon=ICON_NETWORK;
            break;
          case DRIVE_REMOVABLE:
            icon=ICON_FLOPPY_3_5;
            break;
          case DRIVE_FIXED:
            icon=ICON_HDD;
            break;
          default:
            icon=ICON_NETWORK;
            break;
        }
        drive_name[0]='A'+bit_index;
        Add_element_to_list(list, drive_name, Format_filename(drive_name,name_length-1,2), 2, icon);
        list->Nb_directories++;
        drive_index++;
      }
    }
  }
  #elif defined(__MINT__)
    drive_bits = Drvmap(); //get drive map bitfield

    for (bit_index=0; bit_index<32; bit_index++)
    {
      if ( (1 << bit_index) & drive_bits )
      {
        drive_name[0]='A'+bit_index;
        Add_element_to_list(list, drive_name,Format_filename(drive_name,name_length,2),2,ICON_NONE);
        list->Nb_directories++;
        drive_index++;
      }
    }
  #else
  {
    //Sous les différents unix, on va mettre
    // un disque dur qui pointera vers la racine,
    // et un autre vers le home directory de l'utilisateur.

    // Ensuite on utilise read_file_system_list pour compléter

    struct mount_entry* mount_points_list;
    struct mount_entry* next;

    char * home_dir = getenv("HOME");
    Add_element_to_list(list, "/", Format_filename("/",name_length-1,2), 2, ICON_HDD);
    list->Nb_directories++;
    if(home_dir)
    {
        Add_element_to_list(list, home_dir, Format_filename(home_dir, name_length, 2), 2, ICON_NONE);
        list->Nb_directories++;
    }

    mount_points_list = read_file_system_list(0);

    while(mount_points_list != NULL)
    {
      byte icon = ICON_NONE;
      if (mount_points_list->me_remote)
        icon = ICON_NETWORK;
      else if (strcmp(mount_points_list->me_type, "cd9660") == 0)
        icon = ICON_CDROM;
      else if (strcmp(mount_points_list->me_type, "nfs") == 0)
        icon = ICON_NETWORK;
      else if (strcmp(mount_points_list->me_type, "msdos") == 0)
        icon = ICON_FLOPPY_3_5; // Only a guess...
      else if (strcmp(mount_points_list->me_type, "ext2fs") == 0)
        icon = ICON_HDD; // Only a guess...
      else if (strcmp(mount_points_list->me_type, "hfs") == 0)
        icon = ICON_HDD; // Only a guess...
      else if (strcmp(mount_points_list->me_type, "ufs") == 0)
        icon = ICON_HDD; // Only a guess...
      else if (strcmp(mount_points_list->me_type, "zfs") == 0)
        icon = ICON_HDD; // Only a guess...

      GFX2_Log(GFX2_DEBUG, "dummy=%u type=%s path=%s icon=%d\n", mount_points_list->me_dummy, mount_points_list->me_type, mount_points_list->me_mountdir, (int)icon);

      if(mount_points_list->me_dummy == 0 && strcmp(mount_points_list->me_mountdir,"/") && strcmp(mount_points_list->me_mountdir,"/home"))
        {
          Add_element_to_list(list, mount_points_list->me_mountdir,
              Format_filename(mount_points_list->me_mountdir, name_length + (icon==ICON_NONE?0:-1), 2), 2, icon);
          list->Nb_directories++;
        }
        next = mount_points_list -> me_next;
        #if !(defined(__macosx__) || defined(__FreeBSD__))
          free(mount_points_list -> me_type);
        #endif
        free(mount_points_list -> me_devname);
        free(mount_points_list -> me_mountdir);
        free(mount_points_list);
        mount_points_list = next;
    }

  }
  #endif

  Recount_files(list);
}

// Comparison of file names:
#ifdef WIN32
// case-insensitive
  #define FILENAME_COMPARE strcasecmp
#else
// case-sensitive
  #define FILENAME_COMPARE strcmp
#endif


/**
 * Sort a file/directory list.
 * The sord is done in that order :
 * Directories first, in alphabetical order,
 * then Files, in alphabetical order.
 *
 * List counts and index are updated.
 * @param list the linked list
 */
void Sort_list_of_files(T_Fileselector *list)
{
  byte   list_is_sorted; // Booléen "La liste est triée"
  byte   need_swap;          // Booléen "Il faut inverser les éléments"
  T_Fileselector_item * prev_item;
  T_Fileselector_item * current_item;
  T_Fileselector_item * next_item;
  T_Fileselector_item * next_to_next_item;

  // Check there are at least two elements before sorting
  if (list->First && list->First->Next)
  {
    do
    {
      // Par défaut, on considère que la liste est triée
      list_is_sorted=1;

      current_item=list->First;
      next_item=current_item->Next;

      while ( (current_item!=NULL) && (next_item!=NULL) )
      {
        // On commence par supposer qu'il n'y pas pas besoin d'inversion
        need_swap=0;

        // Ensuite, on vérifie si les deux éléments sont bien dans l'ordre ou
        // non:

          // Drives go at the top of the list, and files go after them
        if ( current_item->Type < next_item->Type )
          need_swap=1;
          // If both elements have the same type, compare the file names, if
		  // current is alphabetically before, we need to swap, unless it is
          // parent directory, which should always go first
        else if ( (current_item->Type==next_item->Type) &&
                  (((FILENAME_COMPARE(current_item->Full_name,next_item->Full_name)>0) &&
				  (FILENAME_COMPARE(current_item->Full_name, PARENT_DIR) != 0)) ||
				   (FILENAME_COMPARE(next_item->Full_name, PARENT_DIR) == 0))
				  )
          need_swap=1;


        if (need_swap)
        {
          // Si les deux éléments nécessitent d'être inversé:

          // On les inverses:

          // On note avant tout les éléments qui encapsulent nos deux amis
          prev_item         =current_item->Previous;
          next_to_next_item=next_item->Next;

          // On permute le chaînage des deux éléments entree eux
          current_item->Next  =next_to_next_item;
          current_item->Previous=next_item;
          next_item->Next  =current_item;
          next_item->Previous=prev_item;

          // On tente un chaînage des éléments encapsulant les compères:
          if (prev_item!=NULL)
            prev_item->Next=next_item;
          if (next_to_next_item!=NULL)
            next_to_next_item->Previous=current_item;

          // On fait bien attention à modifier la tête de liste en cas de besoin
          if (current_item==list->First)
            list->First=next_item;

          // Ensuite, on se prépare à étudier les éléments précédents:
          current_item=prev_item;

          // Et on constate que la liste n'était pas encore génialement triée
          list_is_sorted=0;
        }
        else
        {
          // Si les deux éléments sont dans l'ordre:

          // On passe aux suivants
          current_item=current_item->Next;
          next_item=next_item->Next;
        }
      }
    }
    while (!list_is_sorted);
  }
  // Force a recount / re-index
  Recount_files(list);
}

T_Fileselector_item * Get_item_by_index(T_Fileselector *list, unsigned short index)
{
  // Safety
  if (list->Nb_elements == 0)
    return NULL;
  if (index >= list->Nb_elements)
    index=list->Nb_elements-1;

  if (list->Index)
  {
    return list->Index[index];
  }
  else
  {
    // Index not available.
    // Can only happen in case of malloc error.
    // Fall back anyway on iterative search
 
    T_Fileselector_item * item = list->First;
    for (; index > 0 && item != NULL; index--)
      item = item->Next;
    
    return item;
  }

}


/**
 * Display of the file/directory list items.
 * @param list the file list
 * @param offset_first offset between the 1st visible file and the first file in list.
 * @param selector_offset offset between the 1st visible file and the selected file.
 */
void Display_file_list(T_Fileselector *list, short offset_first,short selector_offset)
{
  T_Fileselector_item * current_item;
  byte   index;  // index du fichier qu'on affiche (0 -> 9)
  byte   text_color;
  byte   background_color;


  // On vérifie s'il y a au moins 1 fichier dans la liste:
  if (list->Nb_elements>0)
  {
    // On commence par chercher à pointer sur le premier fichier visible:
    current_item = Get_item_by_index(list, offset_first);

    // Pour chacun des 10 éléments inscriptibles à l'écran
    for (index=0;index<10;index++)
    {
      // S'il est sélectionné:
      if (!selector_offset)
      {
        // Si c'est un fichier
        if (current_item->Type==0)
          text_color=SELECTED_FILE_COLOR;
        else
          text_color=SELECTED_DIRECTORY_COLOR;

        background_color=SELECTED_BACKGROUND_COLOR;
      }
      else
      {
        // Si c'est un fichier
        if (current_item->Type==0)
          text_color=NORMAL_FILE_COLOR;
        else
          text_color=NORMAL_DIRECTORY_COLOR;

        background_color=NORMAL_BACKGROUND_COLOR;
      }

      // On affiche l'élément
      if (current_item->Icon != ICON_NONE)
      {
        // Name preceded by an icon
        Print_in_window(16,95+index*8,current_item->Short_name,text_color,background_color);
        Window_display_icon_sprite(8,95+index*8,current_item->Icon);
      }
      else
      {
        // Name without icon
        if (current_item->Unicode_short_name)
          Print_in_window_unicode(8,95+index*8,current_item->Unicode_short_name,text_color,background_color);
        else
          Print_in_window(8,95+index*8,current_item->Short_name,text_color,background_color);
      }

      // On passe à la ligne suivante
      selector_offset--;
      current_item=current_item->Next;
      if (!current_item)
        break;
    } // End de la boucle d'affichage

  } // End du test d'existence de fichiers
}


/**
 * Get the label of a list item.
 * @param list the file list
 * @param offset_first offset between the 1st visible file and the 1st file in list.
 * @param selector_offset offset between the 1st visible file and the wanted label.
 * @param label pointer to a buffer to receive the label (ANSI)
 * @param unicode_label pointer to a buffer to receive the label (Unicode)
 * @param type NULL or a pointer to receive the type : 0 = file, 1 = directory, 2 = drive.
 */
static void Get_selected_item(T_Fileselector *list, short offset_first,short selector_offset,char * label,word * unicode_label,int *type)
{
  T_Fileselector_item * current_item;

  // On vérifie s'il y a au moins 1 fichier dans la liste:
  if (list->Nb_elements>0)
  {
    // On commence par chercher à pointer sur le premier fichier visible:
    // Ensuite, on saute autant d'éléments que le décalage demandé:
    current_item = Get_item_by_index(list, offset_first + selector_offset);

    // On recopie la chaîne
    strcpy(label, current_item->Full_name);
    if (unicode_label != NULL)
    {
      if (current_item->Unicode_full_name)
        Unicode_strlcpy(unicode_label, current_item->Unicode_full_name, 256);
      else
        Unicode_char_strlcpy(unicode_label, current_item->Full_name, 256);
    }

    if (type != NULL)
      *type=current_item->Type;
  } // End du test d'existence de fichiers
}


// ----------------- Déplacements dans la liste de fichiers -----------------

void Selector_scroll_down(short * offset_first,short * selector_offset)
// Fait scroller vers le bas le sélecteur de fichier... (si possible)
{
  if ( ((*selector_offset)<9)
    && ( (*selector_offset)+1 < Filelist.Nb_elements ) )
    // Si la sélection peut descendre
    Display_file_list(&Filelist, *offset_first,++(*selector_offset));
  else // Sinon, descendre la fenêtre (si possible)
  if ((*offset_first)+10<Filelist.Nb_elements)
    Display_file_list(&Filelist, ++(*offset_first),*selector_offset);
}


void Selector_scroll_up(short * offset_first,short * selector_offset)
// Fait scroller vers le haut le sélecteur de fichier... (si possible)
{
  if ((*selector_offset)>0)
    // Si la sélection peut monter
    Display_file_list(&Filelist, *offset_first,--(*selector_offset));
  else // Sinon, monter la fenêtre (si possible)
  if ((*offset_first)>0)
    Display_file_list(&Filelist, --(*offset_first),*selector_offset);
}


void Selector_page_down(short * offset_first,short * selector_offset, short lines)
{
  if (Filelist.Nb_elements-1>*offset_first+*selector_offset)
  {
    if (*selector_offset<9)
    {
      if (Filelist.Nb_elements<10)
      {
        *offset_first=0;
        *selector_offset=Filelist.Nb_elements-1;
      }
      else *selector_offset=9;
    }
    else
    {
      if (Filelist.Nb_elements>*offset_first+18)
        *offset_first+=lines;
      else
      {
        *offset_first=Filelist.Nb_elements-10;
        *selector_offset=9;
      }
    }
  }
  Display_file_list(&Filelist, *offset_first,*selector_offset);
}


void Selector_page_up(short * offset_first,short * selector_offset, short lines)
{
  if (*offset_first+*selector_offset>0)
  {
    if (*selector_offset>0)
      *selector_offset=0;
    else
    {
      if (*offset_first>lines)
        *offset_first-=lines;
      else
        *offset_first=0;
    }
  }
  Display_file_list(&Filelist, *offset_first,*selector_offset);
}


void Selector_end(short * offset_first,short * selector_offset)
{
  if (Filelist.Nb_elements<10)
  {
    *offset_first=0;
    *selector_offset=Filelist.Nb_elements-1;
  }
  else
  {
    *offset_first=Filelist.Nb_elements-10;
    *selector_offset=9;
  }
  Display_file_list(&Filelist, *offset_first,*selector_offset);
}


void Selector_home(short * offset_first,short * selector_offset)
{
  Display_file_list(&Filelist, (*offset_first)=0,(*selector_offset)=0);
}



short Compute_click_offset_in_fileselector(void)
/*
  Renvoie le décalage dans le sélecteur de fichier sur lequel on a clické.
  Renvoie le décalage du dernier fichier si on a clické au delà.
  Renvoie -1 si le sélecteur est vide.
*/
{
  short computed_offset;

  computed_offset=(((Mouse_Y-Window_pos_Y)/Menu_factor_Y)-95)>>3;
  if (computed_offset>=Filelist.Nb_elements)
    computed_offset=Filelist.Nb_elements-1;

  return computed_offset;
}

void Display_bookmark(T_Dropdown_button * Button, int bookmark_number)
{
  if (Config.Bookmark_directory[bookmark_number])
  {
    int label_size;
    // Libellé
    Print_in_window_limited(Button->Pos_X+3+10,Button->Pos_Y+2,Config.Bookmark_label[bookmark_number],8,MC_Black,MC_Light);
    label_size=strlen(Config.Bookmark_label[bookmark_number]);
    if (label_size<8)
      Window_rectangle(Button->Pos_X+3+10+label_size*8,Button->Pos_Y+2,(8-label_size)*8,8,MC_Light);
    // Menu apparait sur clic droit
    Button->Active_button=RIGHT_SIDE;
    // item actifs
    Window_dropdown_clear_items(Button);
    Window_dropdown_add_item(Button,0,"Set");
    Window_dropdown_add_item(Button,1,"Rename");
    Window_dropdown_add_item(Button,2,"Clear");    
  }
  else
  {
    // Libellé
    Print_in_window(Button->Pos_X+3+10,Button->Pos_Y+2,"--------",MC_Dark,MC_Light);
    // Menu apparait sur clic droit ou gauche
    Button->Active_button=RIGHT_SIDE|LEFT_SIDE;
    // item actifs
    Window_dropdown_clear_items(Button);
    Window_dropdown_add_item(Button,0,"Set");
  }
}

//------------------------ Chargements et sauvegardes ------------------------

void Print_current_directory(void)
//
// Shows Selector->Directory on 37 chars
//
{
  char converted_name[MAX_PATH_CHARACTERS];
  int  length; // length du répertoire courant
  int  index;   // index de parcours de la chaine complète

  strncpy(converted_name,Selector->Directory,sizeof(converted_name));
  converted_name[sizeof(converted_name)-1] = '\0';
#ifdef ENABLE_FILENAMES_ICONV
  {
    /* convert file name from UTF8 to ANSI */
    char * input = Selector->Directory;
    size_t inbytesleft = strlen(input);
    char * output = converted_name;
    size_t outbytesleft = sizeof(converted_name)-1;
    if(cd != (iconv_t)-1 && (ssize_t)iconv(cd, &input, &inbytesleft, &output, &outbytesleft) >= 0)
      *output = '\0';
  }
#endif /* ENABLE_FILENAMES_ICONV */

  Window_rectangle(10,84,37*8,8,MC_Light);

  if (Selector->Directory_unicode[0] != 0)
  {
    length=Unicode_strlen(Selector->Directory_unicode);
    if (length>MAX_DISPLAYABLE_PATH)
    { // We need to truncate the directory
      word temp_name[MAX_DISPLAYABLE_PATH+1]; // truncated name

      memcpy(temp_name, Selector->Directory_unicode, 3*2);  // first 3 chars "C:\"
      temp_name[3] = (byte)ELLIPSIS_CHARACTER;
      temp_name[4] = 0;

      // next we look for a place to fit everything ;)
      for (index=3;index<length;index++)
        if ( (Selector->Directory_unicode[index]==PATH_SEPARATOR[0]) &&
            (length-index<=MAX_DISPLAYABLE_PATH-4) )
        {
          // we found the place !
          Unicode_strlcpy(temp_name+4,Selector->Directory_unicode+index, MAX_DISPLAYABLE_PATH+1-4);
          break;
        }

      Print_in_window_unicode(10,84,temp_name,MC_Black,MC_Light);
    }
    else // The string is short enough
      Print_in_window_unicode(10,84,Selector->Directory_unicode,MC_Black,MC_Light);
  }
  else
  {
    length=strlen(converted_name);
    if (length>MAX_DISPLAYABLE_PATH)
    { // We need to truncate the directory
      char temp_name[MAX_DISPLAYABLE_PATH+1]; // truncated name

      for (index=0;index<3;index++) // copy the first 3 chars "C:\"
        temp_name[index]=converted_name[index];

      temp_name[3] = ELLIPSIS_CHARACTER;
      temp_name[4] = '\0';

      // next we look for a place to fit everything ;)
      for (index++;index<length;index++)
        if ( (converted_name[index]==PATH_SEPARATOR[0]) &&
            (length-index<=MAX_DISPLAYABLE_PATH-4) )
        {
          // we found the place !
          strcpy(temp_name+4,converted_name+index);
          break;
        }

      // display truncated string
      Print_in_window(10,84,temp_name,MC_Black,MC_Light);
    }
    else // The string is short enough
      Print_in_window(10,84,converted_name,MC_Black,MC_Light);
  }
    
  Update_window_area(10,84,37*8,8);
}

//
// Print the current file name
//
void Print_filename_in_fileselector(void)
{
  char filename[32];
  strncpy(filename, Selector_filename, sizeof(filename));
#ifdef ENABLE_FILENAMES_ICONV
  {
    char * input = (char *)Selector_filename;
    size_t inbytesleft = strlen(Selector_filename);
    char * output = filename;
    size_t outbytesleft = sizeof(filename)-1;
    if(cd != (iconv_t)-1 && (ssize_t)iconv(cd, &input, &inbytesleft, &output, &outbytesleft) >= 0)
      *output = '\0';
  }
#endif /* ENABLE_FILENAMES_ICONV */
  Window_rectangle(82,48,27*8,8,MC_Light);
  if (Selector_filename_unicode[0] != 0)
  {
    word filename_unicode[32];
    Unicode_strlcpy(filename_unicode, Selector_filename_unicode, 28); // 28 including the terminating 0
    Print_in_window_unicode(82,48,filename_unicode,MC_Black,MC_Light);
  }
  else
    Print_in_window_limited(82,48,filename,27,MC_Black,MC_Light);
  Update_window_area(82,48,27*8,8);
}

static int Selected_type; // Utilisé pour mémoriser le type d'entrée choisi
                        // dans le selecteur de fichier.

/// Displays the file list with sliders, etc.
/// also optionally updates the current file name (Selector_filename)
/// @param Position the current position in the file list
/// @param offset   the offset of the selected item in the file list
/// @param button   the scrollbar/slider GUI control
/// @param setfilename option to update Selector_filename
static void Prepare_and_display_filelist(short Position, short offset, T_Scroller_button * button, int setfilename)
{
  button->Nb_elements=Filelist.Nb_elements;
  button->Position=Position;
  Compute_slider_cursor_length(button);
  Window_draw_slider(button);
  // On efface les anciens noms de fichier:
  Window_rectangle(8-1,95-1,144+2,80+2,MC_Black);
  // On affiche les nouveaux:
  Display_file_list(&Filelist, Position,offset);

  Update_window_area(8-1,95-1,144+2,80+2);

  if (setfilename)
  {
    // On récupère le nom du schmilblick à "accéder"
    Get_selected_item(&Filelist, Position,offset,Selector_filename,Selector_filename_unicode,&Selected_type);
  }
  // On affiche le nouveau nom de fichier
  Print_filename_in_fileselector();
  // On affiche le nom du répertoire courant
  Print_current_directory();
}


static void Reload_list_of_files(byte filter, T_Scroller_button * button)
{
  Read_list_of_files(&Filelist, filter);
  Sort_list_of_files(&Filelist);
  //
  // Check and fix the fileselector positions, because 
  // the directory content may have changed.
  //
  // Make the offset absolute
  Selector->Offset += Selector->Position;
  // Ensure it's within limits
  if (Selector->Offset >= Filelist.Nb_elements)
  {
    Selector->Offset = Filelist.Nb_elements-1;
  }
  // Ensure the position doesn't show "too many files"
  if (Selector->Position!=0 && Selector->Position>(Filelist.Nb_elements-10))
  {
    if (Filelist.Nb_elements<10)
    {
      Selector->Position=0;
    }
    else
    {
      Selector->Position=Filelist.Nb_elements-10;
    }
  }
  // Restore the offset as relative to the position.
  Selector->Offset -= Selector->Position;

  Prepare_and_display_filelist(Selector->Position,Selector->Offset,button,1);
}

void Scroll_fileselector(T_Scroller_button * file_scroller)
{
  char old_filename[MAX_PATH_CHARACTERS];

  strcpy(old_filename,Selector_filename);

  // On regarde si la liste a bougé
  if (file_scroller->Position!=Selector->Position)
  {
    // Si c'est le cas, il faut mettre à jour la jauge
    file_scroller->Position=Selector->Position;
    Window_draw_slider(file_scroller);
  }
  // On récupére le nom du schmilblick à "accéder"
  Get_selected_item(&Filelist, Selector->Position,Selector->Offset,Selector_filename,Selector_filename_unicode,&Selected_type);
  if (strcmp(old_filename,Selector_filename))
    New_preview_is_needed=1;

  // On affiche le nouveau nom de fichier
  Print_filename_in_fileselector();
  Display_cursor();
}


short Find_file_in_fileselector(T_Fileselector *list, const char * fname)
{
  T_Fileselector_item * item;
  short  index;
  short  close_match = -1;

  index=0;
  for (item=list->First; item!=NULL; item=item->Next)
  {
    if (strcmp(item->Full_name,fname)==0)
      return index; // exact match
    if (strcasecmp(item->Full_name,fname)==0)
      close_match=index;
      
    index++;
  }

  return close_match;
}

/// Set the position and index of the file list according
/// to the selected index
/// @param index index of selected file
static void Highlight_file(short index)
{

  if ((Filelist.Nb_elements<=10) || (index<5))
  {
    Selector->Position=0;
    Selector->Offset=index;
  }
  else
  {
    if (index>=Filelist.Nb_elements-5)
    {
      Selector->Position=Filelist.Nb_elements-10;
      Selector->Offset=index-Selector->Position;
    }
    else
    {
      Selector->Position=index-4;
      Selector->Offset=4;
    }
  }
}


short Find_filename_match(T_Fileselector *list, char * fname)
{
  short best_match;
  T_Fileselector_item * current_item;
  short item_number;
  byte matching_letters=0;
  byte counter;

  best_match=-1;
  item_number=0;
  
  for (current_item=list->First; current_item!=NULL; current_item=current_item->Next)
  {
    if ( (!Config.Find_file_fast)
      || (Config.Find_file_fast==(current_item->Type+1)) )
    {
      // On compare et si c'est mieux, on stocke dans Meilleur_nom
      for (counter=0; fname[counter]!='\0' && tolower(current_item->Full_name[counter])==tolower(fname[counter]); counter++);
      if (counter>matching_letters)
      {
        matching_letters=counter;
        best_match=item_number;
      }
    }
    item_number++;
  }

  return best_match;
}

// Quicksearch system
char  quicksearch_filename[MAX_PATH_CHARACTERS]="";

void Reset_quicksearch(void)
{
  quicksearch_filename[0]='\0';
}

short Quicksearch(T_Fileselector *selector)
{
  int len;
  short most_matching_item;
  
  // Autre => On se place sur le nom de fichier qui correspond
  len=strlen(quicksearch_filename);
  if (Key_ANSI>= ' ' && Key_ANSI < 255 && len<50)
  {
    quicksearch_filename[len]=Key_ANSI;
    quicksearch_filename[len+1]='\0';
    most_matching_item=Find_filename_match(selector, quicksearch_filename);
    if ( most_matching_item >= 0 )
    {
      return most_matching_item;
    }
    *quicksearch_filename=0;
  }
  return -1;
}

// Translated from Highlight_file
void Locate_list_item(T_List_button * list, short selected_item)
{

  // Safety bounds
  if (selected_item<0)
    selected_item=0;
  else if (selected_item>=list->Scroller->Nb_elements)
    selected_item=list->Scroller->Nb_elements-1;
    
    
  if ((list->Scroller->Nb_elements<=list->Scroller->Nb_visibles) || (selected_item<(list->Scroller->Nb_visibles/2)))
  {
    list->List_start=0;
    list->Cursor_position=selected_item;
  }
  else
  {
    if (selected_item>=list->Scroller->Nb_elements-(list->Scroller->Nb_visibles/2))
    {
      list->List_start=list->Scroller->Nb_elements-list->Scroller->Nb_visibles;
      list->Cursor_position=selected_item-list->List_start;
    }
    else
    {
      list->List_start=selected_item-(list->Scroller->Nb_visibles/2-1);
      list->Cursor_position=(list->Scroller->Nb_visibles/2-1);
    }
  }
}

int Quicksearch_list(T_List_button * list, T_Fileselector * selector)
{
  // Try Quicksearch
  short selected_item=Quicksearch(selector);
  if (selected_item>=0 && selected_item!=list->Cursor_position+list->List_start)
  {
    Locate_list_item(list, selected_item);
    
    Hide_cursor();
    // Mise à jour du scroller
    list->Scroller->Position=list->List_start;
    Window_draw_slider(list->Scroller);
    
    Window_redraw_list(list);
    Display_cursor();
    // Store the selected value as attribute2
    Window_attribute2=list->List_start + list->Cursor_position;
    // Return the control ID of the list.
    return list->Number;
  }
  return 0;
}

byte Button_Load_or_Save(T_Selector_settings *settings, byte load, T_IO_Context *context)
  // load=1 => On affiche le menu du bouton LOAD
  // load=0 => On affiche le menu du bouton SAVE
{
  short clicked_button;
  T_Scroller_button * file_scroller;
  T_Dropdown_button * formats_dropdown;
  T_Dropdown_button * bookmark_dropdown[4];
  short temp;
  unsigned int format;
  int dummy=0;       // Sert à appeler SDL_GetKeyState
  byte  save_or_load_image=0;
  byte  has_clicked_ok=0;// Indique si on a clické sur Load ou Save ou sur
                             //un bouton enclenchant Load ou Save juste après.
  byte  initial_back_color; // preview destroys it (how nice)
  char  save_filename[MAX_PATH_CHARACTERS];
  word  save_filename_unicode[MAX_PATH_CHARACTERS];
  char  initial_comment[COMMENT_SIZE+1];
  short window_shortcut;
  const char * directory_to_change_to = NULL;

  Selector=settings;

  Reset_quicksearch();
  
  //if (Native_filesel(load) != 0); // TODO : handle this
  
  if (context->Type == CONTEXT_MAIN_IMAGE)
    window_shortcut = load?(0x100+BUTTON_LOAD):(0x100+BUTTON_SAVE);
  else
    window_shortcut = load?SPECIAL_LOAD_BRUSH:SPECIAL_SAVE_BRUSH;

  // Backup data that needs be restored on "cancel"  
  initial_back_color=Back_color;
  strcpy(initial_comment,context->Comment);
  
  if (load)
  {
    if (context->Type == CONTEXT_MAIN_IMAGE)
      Open_window(310,200,"Load picture");
    else if (context->Type == CONTEXT_PALETTE)
      Open_window(310,200,"Load palette");
    else
      Open_window(310,200,"Load brush");
    Window_set_normal_button(198,180,51,14,"Load",0,1,KEY_RETURN); // 1
  }
  else
  {
    if (context->Type == CONTEXT_MAIN_IMAGE)
      Open_window(310,200,"Save picture");
    else if (context->Type == CONTEXT_BRUSH)
      Open_window(310,200,"Save brush");
    else if (context->Type == CONTEXT_PALETTE)
      Open_window(310,200,"Save palette");
    else
      assert(0);
    Window_set_normal_button(198,180,51,14,"Save",0,1,KEY_RETURN); // 1
    if (Selector->Format_filter<=FORMAT_ALL_FILES) // Correction du *.*
    {
      Selector->Format_filter=context->Format;
      Selector->Position=0;
      Selector->Offset=0;
    }

    if (Get_fileformat(Selector->Format_filter)->Save == NULL) // Correction d'un format insauvable
    {
      Selector->Format_filter=DEFAULT_FILEFORMAT;
      Selector->Position=0;
      Selector->Offset=0;
    }
    // Affichage du commentaire
    if (Get_fileformat(Selector->Format_filter)->Comment)
      Print_in_window(45,70,context->Comment,MC_Black,MC_Light);      
  }

  Window_set_normal_button(253,180,51,14,"Cancel",0,1,KEY_ESC); // 2
  Window_set_normal_button(7,180,51,14,"Delete",0,1,KEY_DELETE); // 3

  // Frame autour des infos sur le fichier de dessin
  Window_display_frame_in(6, 44,299, 37);
  // Frame autour de la preview
  Window_display_frame_in(181,93,124,84);
  // Frame autour du fileselector
  Window_display_frame_in(6,93,148,84);

  // Fileselector
  Window_set_special_button(9,95,144,80,0); // 4

  // Scroller du fileselector
  file_scroller = Window_set_scroller_button(160,94,82,1,10,0);               // 5

  // Dropdown pour les formats de fichier
  formats_dropdown=
    Window_set_dropdown_button(68,28,52,11,0,
      Get_fileformat(Selector->Format_filter)->Label,
      1,0,1,RIGHT_SIDE|LEFT_SIDE,0); // 6

  for (format=0; format < Nb_known_formats(); format++)
  {
    if (File_formats[format].Identifier > FORMAT_ALL_FILES)
    {
      if (load && !File_formats[format].Load) //filter out formats without Load function
        continue;
      if (!load && !File_formats[format].Save) // filter out formats without Save function
        continue;
    }
    if (!load && ((context->Type == CONTEXT_PALETTE) != File_formats[format].Palette_only))
      continue;   // Only Palette only format when SAVING palette and not Palette only when saving image
    Window_dropdown_add_item(formats_dropdown,File_formats[format].Identifier,File_formats[format].Label);
  }
  Print_in_window(70,18,"Format",MC_Dark,MC_Light);
  
  // Texte de commentaire des dessins
  Print_in_window(9,70,"Txt:",MC_Dark,MC_Light);
  Window_set_input_button(43,68,COMMENT_SIZE); // 7

  // Saisie du nom de fichier
  Window_set_input_button(80,46,27); // 8

  Print_in_window(9,47,"Filename",MC_Dark,MC_Light);
  Print_in_window(9,59,"Image:",MC_Dark,MC_Light);
  Print_in_window(228,59,"(",MC_Dark,MC_Light);
  Print_in_window(292,59,")",MC_Dark,MC_Light);

  // Selecteur de Lecteur / Volume
  Window_set_normal_button(7,18,53,23,"",0,1,KEY_NONE); // 9
  Print_in_window(10,22,"Select",MC_Black,MC_Light);
  Print_in_window(14,30,"drive",MC_Black,MC_Light);
 
  // Bookmarks
  for (temp=0;temp<NB_BOOKMARKS;temp++)
  {
    bookmark_dropdown[temp]=
      Window_set_dropdown_button(127+(88+1)*(temp%2),18+(temp/2)*12,88,11,56,"",0,0,1,RIGHT_SIDE,0); // 10-13
    Window_display_icon_sprite(bookmark_dropdown[temp]->Pos_X+3,bookmark_dropdown[temp]->Pos_Y+2,ICON_STAR);
    Display_bookmark(bookmark_dropdown[temp],temp);
  }
  
  Change_directory(context->File_directory);
  Get_current_directory(Selector->Directory, Selector->Directory_unicode, MAX_PATH_CHARACTERS);
  
  // Affichage des premiers fichiers visibles:
  Reload_list_of_files(Selector->Format_filter,file_scroller);

  if (!load)
  {
    short pos = Find_file_in_fileselector(&Filelist, context->File_name);
    Highlight_file((pos >= 0) ? pos : 0);
    Prepare_and_display_filelist(Selector->Position,Selector->Offset,file_scroller,0);

    // On initialise le nom de fichier à celui en cours et non pas celui sous
    // la barre de sélection
    strcpy(Selector_filename,context->File_name);
    Unicode_strlcpy(Selector_filename_unicode, context->File_name_unicode, 256);
    // On affiche le nouveau nom de fichier
    Print_filename_in_fileselector();
  }

  New_preview_is_needed=1;
  Update_window_area(0,0,Window_width, Window_height);

  Display_cursor();

  do
  {
    clicked_button=Window_clicked_button();

    switch (clicked_button)
    {
      case -1 :
      case  0 :
        break;

      case  1 : // Load ou Save
      if(load)
        {
          // Determine the type
          if(File_exists(Selector_filename)) 
          {
            Selected_type = 0;
            if(Directory_exists(Selector_filename)) Selected_type = 1;
          }
          else
          {
            Selected_type = 1;
          }
        }
        else
        {
          if(Directory_exists(Selector_filename)) Selected_type = 1;
          else Selected_type = 0;
        }
        has_clicked_ok=1;
        break;

      case  2 : // Cancel
        break;

      case  3 : // Delete
        if (Filelist.Nb_elements && (*Selector_filename!='.') && Selected_type!=2)
        {
          char * message;
          Hide_cursor();
          // On affiche une demande de confirmation
          if (Selector->Position+Selector->Offset>=Filelist.Nb_directories)
          {
            message="Delete file ?";
          }
          else
          {
            message="Remove directory ?";
          }
          if (Confirmation_box(message))
          {
            // Si c'est un fichier
            if (Selector->Position+Selector->Offset>=Filelist.Nb_directories)
              // On efface le fichier (si on peut)
              temp=(!Remove_path(Selector_filename));
            else // Si c'est un repertoire
              // On efface le repertoire (si on peut)
              temp=(!Remove_directory(Selector_filename));

            if (temp) // temp indique si l'effacement s'est bien passé
            {
              // On remonte si c'était le dernier élément de la liste
              if (Selector->Position+Selector->Offset==Filelist.Nb_elements-1)
              {
                if (Selector->Position)
                  Selector->Position--;
                else
                  if (Selector->Offset)
                    Selector->Offset--;
              }
              else // Si ce n'était pas le dernier, il faut faire gaffe à ce
              {    // que ses copains d'en dessous ne remontent pas trop.
                if ( (Selector->Position)
                  && (Selector->Position+10==Filelist.Nb_elements) )
                  {
                    Selector->Position--;
                    Selector->Offset++;
                  }
              }
              // On relit les informations
              Reload_list_of_files(Selector->Format_filter,file_scroller);
              // On demande la preview du nouveau fichier sur lequel on se trouve
              New_preview_is_needed=1;
            }
            else
              Error(0);
          }
        }
        break;

      case  4 : // Zone d'affichage de la liste de fichiers
        Hide_cursor();

        temp=Compute_click_offset_in_fileselector();
        if (temp>=0)
        {
          if (temp!=Selector->Offset)
          {
            // On met à jour le décalage
            Selector->Offset=temp;

            // On récupére le nom du schmilblick à "accéder"
            Get_selected_item(&Filelist, Selector->Position,Selector->Offset,Selector_filename,Selector_filename_unicode,&Selected_type);
            // On affiche le nouveau nom de fichier
            Print_filename_in_fileselector();
            // On affiche à nouveau la liste
            Display_file_list(&Filelist, Selector->Position,Selector->Offset);

            // On vient de changer de nom de fichier, donc on doit s'appreter
            // a rafficher une preview
            New_preview_is_needed=1;
            Reset_quicksearch();
            
          }
          else
          {
            //   En sauvegarde, si on a double-clické sur un répertoire, il
            // faut mettre le nom de fichier au nom du répertoire. Sinon, dans
            // certains cas, on risque de sauvegarder avec le nom du fichier
            // actuel au lieu de changer de répertoire.
            if (Selector->Position+Selector->Offset<Filelist.Nb_directories)
              Get_selected_item(&Filelist, Selector->Position,Selector->Offset,Selector_filename,Selector_filename_unicode,&Selected_type);

            has_clicked_ok=1;
            New_preview_is_needed=1;
            Reset_quicksearch();
          }
        }
        Display_cursor();
        Wait_end_of_click();
        break;

      case  5 : // Scroller de fichiers
        Hide_cursor();
        Selector->Position=Window_attribute2;
        // On récupére le nom du schmilblick à "accéder"
        Get_selected_item(&Filelist, Selector->Position,Selector->Offset,Selector_filename,Selector_filename_unicode,&Selected_type);
        // On affiche le nouveau nom de fichier
        Print_filename_in_fileselector();
        // On affiche à nouveau la liste
        Display_file_list(&Filelist, Selector->Position,Selector->Offset);
        Display_cursor();
        New_preview_is_needed=1;
        Reset_quicksearch();
        break;

    case  6 : // File Format dropdown
        // Refresh fileselector according to new filter
        if (Selector->Format_filter != Window_attribute2)
        {
          int pos_last_dot;
          char* savename = NULL;
          word Selector_filename_unicode_save[256];

          GFX2_Log(GFX2_DEBUG, "fileselector format changed from %d to %d\n", (int)Selector->Format_filter, (int)Window_attribute2);
          Selector->Format_filter = Window_attribute2;
          if (!load)
          {
            // In "save" box, if current name is a (possible) filename
            // with extension, set it to new format's extension
            pos_last_dot = Position_last_dot(Selector_filename);
            if (Get_fileformat(Selector->Format_filter)->Default_extension[0] != '\0' &&
              pos_last_dot!=-1 &&
              Selector_filename[pos_last_dot+1]!='\0')
            {
              GFX2_Log(GFX2_DEBUG, "extension %s => %s\n", Selector_filename + pos_last_dot + 1, Get_fileformat(Selector->Format_filter)->Default_extension);
              strcpy(Selector_filename + pos_last_dot + 1,
                Get_fileformat(Selector->Format_filter)->Default_extension);
              pos_last_dot = Position_last_dot_unicode(Selector_filename_unicode);
              if (pos_last_dot != -1)
                Unicode_char_strlcpy(Selector_filename_unicode + pos_last_dot + 1,
                  Get_fileformat(Selector->Format_filter)->Default_extension, 256 - pos_last_dot - 1);
            }
          }
          savename = (char *)strdup(Selector_filename);
          memcpy(Selector_filename_unicode_save, Selector_filename_unicode, sizeof(Selector_filename_unicode_save));
          // By default, position list at the beginning
          Selector->Position = 0;
          Selector->Offset = 0;
          // Print the first visible files
          Hide_cursor();
          Reload_list_of_files(Selector->Format_filter, file_scroller);
          New_preview_is_needed = 1;
          Reset_quicksearch();
          if (savename != NULL)
          {
            // attempt to find the file name in new list
            short pos=Find_file_in_fileselector(&Filelist, savename);
            if (pos >= 0)
            {
              Highlight_file(pos);
              Prepare_and_display_filelist(Selector->Position,Selector->Offset,file_scroller,load);
            }
            // If the file is (still present) or it's a name with new
            // extension, set it as the proposed file name.
            if (pos >= 0 || !load)
            {
              strcpy(Selector_filename, savename);
              memcpy(Selector_filename_unicode, Selector_filename_unicode_save, sizeof(Selector_filename_unicode_save));
            }
            free(savename);
          }
          Print_filename_in_fileselector();
          Display_cursor();
        }
        break;
      case  7 : // Saisie d'un commentaire pour la sauvegarde
        if ( (!load) && (Get_fileformat(Selector->Format_filter)->Comment) )
        {
          Readline(45, 70, context->Comment, 32, INPUT_TYPE_STRING);
          Display_cursor();
        }
        break;
      case  8 : // Saisie du nom de fichier
      {
        char filename_ansi[256];
        word filename_unicode[256];

        // Save the filename
        strcpy(save_filename, Selector_filename);
        Unicode_strlcpy(save_filename_unicode, Selector_filename_unicode, MAX_PATH_CHARACTERS);
        // Check if the selected entry is a drive/directory :
        // in, this case, clear the filename
        if (Filelist.Nb_elements>0)
        {
          T_Fileselector_item * current_item;
          current_item = Get_item_by_index(&Filelist, Selector->Position + Selector->Offset);
          if (current_item->Type != 0 && !FILENAME_COMPARE(current_item->Full_name,Selector_filename))
          {
            // current name is a highlighted directory
            Selector_filename[0]='\0';
            Selector_filename_unicode[0]=0;
          }
        }
        strncpy(filename_ansi, Selector_filename, sizeof(filename_ansi));
        if (Selector_filename_unicode[0] == 0 && strlen(Selector_filename) > 0)
          Unicode_char_strlcpy(filename_unicode, Selector_filename, sizeof(filename_unicode)/sizeof(word));
        else
          Unicode_strlcpy(filename_unicode, Selector_filename_unicode, sizeof(filename_unicode)/sizeof(word));
#ifdef ENABLE_FILENAMES_ICONV
        { /* convert from UTF8 to ANSI */
          char * input = (char *)Selector_filename;
          size_t inbytesleft = strlen(input);
          char * output = filename_ansi;
          size_t outbytesleft = sizeof(filename_ansi)-1;
          if(cd != (iconv_t)-1 && (ssize_t)iconv(cd, &input, &inbytesleft, &output, &outbytesleft) >= 0)
            *output = '\0';
        }
#endif /* ENABLE_FILENAMES_ICONV */
#if defined(WIN32) || defined(ENABLE_FILENAMES_ICONV)
        if (Readline_ex_unicode(82,48,filename_ansi,filename_unicode,27,sizeof(filename_ansi)-1,INPUT_TYPE_FILENAME,0))
#else
        if (Readline_ex_unicode(82,48,filename_ansi,NULL,27,sizeof(filename_ansi)-1,INPUT_TYPE_FILENAME,0))
#endif
        {
#if defined(WIN32)
          WCHAR temp_str[MAX_PATH_CHARACTERS];
          if (GetShortPathNameW((WCHAR *)filename_unicode, temp_str, MAX_PATH_CHARACTERS) == 0)
          {
            // generate a temporary ansi name
            int i;
            for (i = 0; i < MAX_PATH_CHARACTERS - 1 && filename_unicode[i] != 0; i++)
            {
              filename_ansi[i] = (filename_unicode[i] < 256) ? (byte)filename_unicode[i] : '_';
            }
            filename_ansi[i] = '\0';
          }
          else
          {
            int i;
            for (i = 0; i < MAX_PATH_CHARACTERS - 1 && temp_str[i] != 0; i++)
              filename_ansi[i] = temp_str[i];
            filename_ansi[i] = '\0';
          }
#elif defined(ENABLE_FILENAMES_ICONV)
          /* convert back from UTF16 to UTF8 */
          char * input = (char *)filename_unicode;
          size_t inbytesleft = 2 * Unicode_strlen(filename_unicode);
          char * output = Selector_filename;
          size_t outbytesleft = sizeof(Selector_filename)-1;
          if(cd_utf16_inv != (iconv_t)-1 && (ssize_t)iconv(cd_utf16_inv, &input, &inbytesleft, &output, &outbytesleft) >= 0)
            *output = '\0';
          else
#endif
            strncpy(Selector_filename, filename_ansi, sizeof(Selector_filename));

#if defined(WIN32) || defined(ENABLE_FILENAMES_ICONV)
          Unicode_strlcpy(Selector_filename_unicode, filename_unicode, sizeof(Selector_filename_unicode)/sizeof(word));
#endif
          //   On regarde s'il faut rajouter une extension. C'est-à-dire s'il
          // n'y a pas de '.' dans le nom du fichier.
          for(temp=0,dummy=0; ((Selector_filename[temp]) && (!dummy)); temp++)
            if (Selector_filename[temp]=='.')
              dummy=1;
          if (!dummy)
          {
            if(!Directory_exists(Selector_filename))
            {
              const char * ext = Get_fileformat(Selector->Format_filter)->Default_extension;
              // put default extension
              // (but maybe we should browse through all available ones until we find
              //  something suitable ?)
              if (ext == NULL)
                ext = "pkm";
              strcat(Selector_filename, ".");
              strcat(Selector_filename, ext);
              Unicode_char_strlcat(Selector_filename_unicode, ".", sizeof(Selector_filename_unicode)/sizeof(word));
              Unicode_char_strlcat(Selector_filename_unicode, ext, sizeof(Selector_filename_unicode)/sizeof(word));
            }
          }
          if(load)
          {
            // Determine the type
            if(File_exists(Selector_filename))
            {
              Selected_type = 0;
              if(Directory_exists(Selector_filename)) Selected_type = 1;
            }
            else
            {
              Selected_type = 1;
            }
          }
          else
          {
            if(Directory_exists(Selector_filename)) Selected_type = 1;
            else Selected_type = 0;
          }

          // Now load immediately, but only if the user exited readline by pressing ENTER
          if (Mouse_K == 0) has_clicked_ok = 1;
        }
        else
        {
          // Restore the old filename
          strcpy(Selector_filename, save_filename);
          Unicode_strlcpy(Selector_filename_unicode, save_filename_unicode, sizeof(Selector_filename_unicode)/sizeof(word));
        }
        Print_filename_in_fileselector();
        Display_cursor();
        break;
      }
      case  9 : // Volume Select
          Hide_cursor();
          //   Comme on tombe sur un disque qu'on connait pas, on se place en
          // début de liste:
          Selector->Position=0;
          Selector->Offset=0;
          // Affichage des premiers fichiers visibles:
          Read_list_of_drives(&Filelist,19);
          Sort_list_of_files(&Filelist);
          Prepare_and_display_filelist(Selector->Position,Selector->Offset,file_scroller,0);
          Display_cursor();
          New_preview_is_needed=1;
          Reset_quicksearch();
          break;
      default:
          if (clicked_button>=10 && clicked_button<10+NB_BOOKMARKS)
          {
            // Bookmark
            char * directory_name;
            
            switch(Window_attribute2)
            {
              case -1: // bouton lui-même: aller au répertoire mémorisé
                if (Config.Bookmark_directory[clicked_button-10])
                {
                  GFX2_Log(GFX2_DEBUG,"Go to bookmark %s\n", Config.Bookmark_directory[clicked_button-10]);
                  // backup the currently selected filename
                  strncpy(save_filename, Selector_filename, sizeof(save_filename));
                  // simulate a click on the bookmarked directory
                  directory_to_change_to = Config.Bookmark_directory[clicked_button-10];
                  Reset_quicksearch();
                }
                break;
                
              case 0: // Set
                free(Config.Bookmark_directory[clicked_button-10]);
                Config.Bookmark_directory[clicked_button-10] = NULL;
                Config.Bookmark_label[clicked_button-10][0]='\0';
                temp=strlen(Selector->Directory);
                Config.Bookmark_directory[clicked_button-10]=malloc(temp+1);
                strcpy(Config.Bookmark_directory[clicked_button-10],Selector->Directory);
                
                directory_name=Find_last_separator(Selector->Directory);
                if (directory_name && directory_name[1]!='\0')
                  directory_name++;
                else
                  directory_name=Selector->Directory;
                temp=strlen(directory_name);
                strncpy(Config.Bookmark_label[clicked_button-10],directory_name,8);
                if (temp>8)
                {
                  Config.Bookmark_label[clicked_button-10][7]=ELLIPSIS_CHARACTER;
                  Config.Bookmark_label[clicked_button-10][8]='\0';
                }
                Display_bookmark(bookmark_dropdown[clicked_button-10],clicked_button-10);
                break;
                
              case 1: // Rename
                if (Config.Bookmark_directory[clicked_button-10])
                {
                  // On enlève les "..." avant l'édition
                  char bookmark_label[8+1];
                  strcpy(bookmark_label, Config.Bookmark_label[clicked_button-10]);
                  if (bookmark_label[7]==ELLIPSIS_CHARACTER)
                    bookmark_label[7]='\0';
                  if (Readline_ex(bookmark_dropdown[clicked_button-10]->Pos_X+3+10,bookmark_dropdown[clicked_button-10]->Pos_Y+2,bookmark_label,8,8,INPUT_TYPE_STRING,0))
                    strcpy(Config.Bookmark_label[clicked_button-10],bookmark_label);
                  Display_bookmark(bookmark_dropdown[clicked_button-10],clicked_button-10);
                  Display_cursor();
                }
                break;

              case 2: // Clear
                if (Config.Bookmark_directory[clicked_button-10] && Confirmation_box("Erase bookmark ?"))
                {
                  free(Config.Bookmark_directory[clicked_button-10]);
                  Config.Bookmark_directory[clicked_button-10]=NULL;
                  Config.Bookmark_label[clicked_button-10][0]='\0';
                  Display_bookmark(bookmark_dropdown[clicked_button-10],clicked_button-10);
                }
                break;
            }
          }
          break;
    }

    switch (Key)
    {
      case KEY_UNKNOWN : break;
      case KEY_DOWN : // Bas
        Reset_quicksearch();
        Hide_cursor();
        Selector_scroll_down(&Selector->Position,&Selector->Offset);
        Scroll_fileselector(file_scroller);
        Key=0;
        break;
      case KEY_UP : // Haut
        Reset_quicksearch();
        Hide_cursor();
        Selector_scroll_up(&Selector->Position,&Selector->Offset);
        Scroll_fileselector(file_scroller);
        Key=0;
        break;
      case KEY_PAGEDOWN : // PageDown
        Reset_quicksearch();
        Hide_cursor();
        Selector_page_down(&Selector->Position,&Selector->Offset,9);
        Scroll_fileselector(file_scroller);
        Key=0;
        break;
      case KEY_PAGEUP : // PageUp
        Reset_quicksearch();
        Hide_cursor();
        Selector_page_up(&Selector->Position,&Selector->Offset,9);
        Scroll_fileselector(file_scroller);
        Key=0;
        break;
      case KEY_END : // End
        Reset_quicksearch();
        Hide_cursor();
        Selector_end(&Selector->Position,&Selector->Offset);
        Scroll_fileselector(file_scroller);
        Key=0;
        break;
      case KEY_HOME : // Home
        Reset_quicksearch();
        Hide_cursor();
        Selector_home(&Selector->Position,&Selector->Offset);
        Scroll_fileselector(file_scroller);
        Key=0;
        break;
      case KEY_MOUSEWHEELDOWN :
        Reset_quicksearch();
        Hide_cursor();
        Selector_page_down(&Selector->Position,&Selector->Offset,3);
        Scroll_fileselector(file_scroller);
        Key=0;
        break;
      case KEY_MOUSEWHEELUP :
        Reset_quicksearch();
        Hide_cursor();
        Selector_page_up(&Selector->Position,&Selector->Offset,3);
        Scroll_fileselector(file_scroller);
        Key=0;
        break;
      case KEY_BACKSPACE : // Backspace
        Reset_quicksearch();
        // Si le choix ".." est bien en tête des propositions...
        if (Filelist.Nb_elements && !strcmp(Filelist.First->Full_name,PARENT_DIR))
        {                              
          // On va dans le répertoire parent.
          strcpy(Selector_filename,PARENT_DIR);
          Selector_filename_unicode[0] = 0;
          Selected_type=1;
          has_clicked_ok=1;
        }
        Key=0;
        break;
      default:
        if (clicked_button<=0)
        {
          short selected_item;
          
          if (Is_shortcut(Key,0x100+BUTTON_HELP))
          {
            Window_help(load?BUTTON_LOAD:BUTTON_SAVE, NULL);
            break;
          }
          if (Is_shortcut(Key,window_shortcut))
          {
            clicked_button=2;
            break;
          }
          
          selected_item=Quicksearch(&Filelist);
          if (selected_item>=0)
          {
              temp=Selector->Position+Selector->Offset;
              Hide_cursor();
              Highlight_file(selected_item);
              Prepare_and_display_filelist(Selector->Position,Selector->Offset,file_scroller,1);
              Display_cursor();
              if (temp!=Selector->Position+Selector->Offset)
                New_preview_is_needed=1;
          }
          // Key=0; ?
        }
        else
          Reset_quicksearch();
    }

    if (has_clicked_ok || (directory_to_change_to != NULL))
    {
      //   Si c'est un répertoire, on annule "has_clicked_ok" et on passe
      // dedans.
      if (Selected_type!=0 || (directory_to_change_to != NULL))
      {
        Hide_cursor();
        has_clicked_ok=0;

        if (directory_to_change_to == NULL)
          directory_to_change_to = Selector_filename;

        // We must enter the directory
        if (Change_directory(directory_to_change_to) == 0)
        {
          short pos;
          char  previous_directory[MAX_PATH_CHARACTERS]; // Répertoire d'où l'on vient après un CHDIR
        #if defined (__MINT__)
          static char path[1024]={0};
          char currentDrive='A';
        #endif

          // save the previous current directory
          if (strcmp(directory_to_change_to,PARENT_DIR) != 0)
          {
            strcpy(previous_directory,PARENT_DIR);
          }
          else
          {
            Extract_filename(previous_directory, Selector->Directory);
          }

          Get_current_directory(Selector->Directory, Selector->Directory_unicode, MAX_PATH_CHARACTERS);
          // read the new directory
          Read_list_of_files(&Filelist, Selector->Format_filter);
          Sort_list_of_files(&Filelist);
          // Set the fileselector bar on the directory we're coming from
          pos = Find_file_in_fileselector(&Filelist, previous_directory);
          Highlight_file((pos >= 0) ? pos : 0);
          // display the 1st visible files
          Prepare_and_display_filelist(Selector->Position,Selector->Offset,file_scroller,0);
          Display_cursor();
          New_preview_is_needed=1;

          // New directory, so we need to reset the quicksearch
          Reset_quicksearch();
        }
        else
        {
          char warn_msg[MAX_PATH_CHARACTERS];
          Display_cursor();
          snprintf(warn_msg, sizeof(warn_msg), "cannot chdir to \"%s\" !", directory_to_change_to);
          Warning(warn_msg);
          // restore Selector_filename
          strncpy(Selector_filename, save_filename, sizeof(Selector_filename));
          Error(0);
        }
        directory_to_change_to = NULL;
      }
      else  // Sinon on essaye de charger ou sauver le fichier
      {
        strcpy(context->File_directory,Selector->Directory);
        context->Format = Selector->Format_filter;
        save_or_load_image=1;
      }
    }

    // Gestion du chrono et des previews
    if (New_preview_is_needed)
    {
      // On efface les infos de la preview précédente s'il y en a une
      // d'affichée
      if (Timer_state==2)
      {
        Hide_cursor();
        // On efface le commentaire précédent
        Window_rectangle(45,70,32*8,8,MC_Light);
        // On nettoie la zone où va s'afficher la preview:
        Window_rectangle(183,95,PREVIEW_WIDTH,PREVIEW_HEIGHT,MC_Light);
        // On efface les dimensions de l'image
        Window_rectangle(101,59,120,8,MC_Light);
        // On efface la taille du fichier
        Window_rectangle(236,59,56,8,MC_Light);
        // On efface le format du fichier
        Window_rectangle(59,59,5*8,8,MC_Light);
        // Affichage du commentaire
        if ( (!load) && (Get_fileformat(Selector->Format_filter)->Comment) )
        {
          Print_in_window(45,70,context->Comment,MC_Black,MC_Light);
        }
        Display_cursor();
        // Un update pour couvrir les 4 zones: 3 libellés plus le commentaire
        Update_window_area(45,48,256,30);
        // Zone de preview
        Update_window_area(183,95,PREVIEW_WIDTH,PREVIEW_HEIGHT);
      }

      New_preview_is_needed=0;
      Timer_state=0;         // State du chrono = Attente d'un Xème de seconde
      // On lit le temps de départ du chrono
      Init_chrono(Config.Timer_delay);
    }

    if (!Timer_state)  // Prendre une nouvelle mesure du chrono et regarder
      Check_timer(); // s'il ne faut pas afficher la preview

    if (Timer_state==1) // Il faut afficher la preview
    {
      if ( (Selector->Position+Selector->Offset>=Filelist.Nb_directories) && (Filelist.Nb_elements) )
      {
        T_IO_Context preview_context;
      
        Init_context_preview(&preview_context, Selector_filename, Selector->Directory);
        preview_context.File_name_unicode = Selector_filename_unicode;
        Hide_cursor();
        if (context->Type == CONTEXT_PALETTE)
          preview_context.Type = CONTEXT_PREVIEW_PALETTE;

        Load_image(&preview_context);
        Destroy_context(&preview_context);

        Update_window_area(0,0,Window_width,Window_height);
        Display_cursor();

      }

      Timer_state=2; // On arrête le chrono
    }
  }
  while ( (!has_clicked_ok) && (clicked_button!=2) );

  if (has_clicked_ok)
  {
    strcpy(context->File_name, Selector_filename);
    if (context->File_name_unicode)
      Unicode_strlcpy(context->File_name_unicode, Selector_filename_unicode, MAX_PATH_CHARACTERS);
    strcpy(context->File_directory, Selector->Directory);
    if (!load)
      context->Format = Selector->Format_filter;
  }
  else
  {
    // Data to restore
    strcpy(context->Comment, initial_comment);
  }
  

  //   On restaure les données de l'image qui ont certainement été modifiées
  // par la preview.
  Back_color=initial_back_color;
  if (Windows_open <= 1)
  {
    // Restore Main.palette only when we are not going back to another window
    // (we let the other window take care of its palette and remapping)
    // This test was added for Load/Save dialog called from the Palette window
    Set_palette(Main.palette);

    Compute_optimal_menu_colors(Main.palette);
  }
  temp=(Window_pos_Y+(Window_height*Menu_factor_Y)<Menu_Y_before_window);

  Close_window();

  if (temp && Windows_open < 1)
    Display_menu();

  Unselect_button((load)?BUTTON_LOAD:BUTTON_SAVE);
  Display_cursor();
  Free_fileselector_list(&Filelist);

  return save_or_load_image;
}
