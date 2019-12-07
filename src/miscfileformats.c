/* vim:expandtab:ts=2 sw=2:
*/
/*  Grafx2 - The Ultimate 256-color bitmap paint program

    Copyright 2018-2019 Thomas Bernard
    Copyright 2011 Pawel Góralski
    Copyright 2009 Petter Lindquist
    Copyright 2008 Yves Rizoud
    Copyright 2008 Franck Charlet
    Copyright 2007-2011 Adrien Destugues
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

///@file miscfileformats.c
/// Formats that aren't fully saving, either because of palette restrictions or other things

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <stdio.h>
#if _MSC_VER < 1900
#define snprintf _snprintf
#endif
#define strdup _strdup
#endif

#include <zlib.h>

#include "engine.h"
#include "errors.h"
#include "global.h"
#include "io.h"
#include "libraw2crtc.h"
#include "loadsave.h"
#include "loadsavefuncs.h"
#include "misc.h"
#include "screen.h"
#include "struct.h"
#include "windows.h"
#include "oldies.h"
#include "c64load.h"
#include "pages.h"
#include "keycodes.h"
#include "input.h"
#include "help.h"
#include "fileformats.h"
#include "gfx2mem.h"
#include "gfx2log.h"

//////////////////////////////////// PAL ////////////////////////////////////
//

// -- Test wether a file is in PAL format --------------------------------
void Test_PAL(T_IO_Context * context, FILE * file)
{
  char buffer[32];
  long file_size;

  (void)context;
  File_error = 1;

  file_size = File_length_file(file);
  // First check for GrafX2 legacy palette format. The simplest one, 768 bytes
  // of RGB data. It is a raw dump of the T_Palette structure. There is no
  // header at all, so we check for the file size.
  if (file_size == sizeof(T_Palette))
    File_error = 0;
  else if (file_size > 8)
  {
    // Bigger (or smaller ?) files may be in other formats. These have an
    // header, so look for it.
    if (!Read_bytes(file, buffer, 8))
      return;
    if (strncmp(buffer,"JASC-PAL",8) == 0)
    {
      // JASC file format, used by Paint Shop Pro and GIMP. This is also the
      // one used for saving, as it brings greater interoperability.
      File_error = 0;
    }
    else if(strncmp(buffer,"RIFF", 4) == 0)
    {
      // Microsoft RIFF file
      // This is a data container (similar to IFF). We only check the first
      // chunk header, and give up if that's not a palette.
      fseek(file, 8, SEEK_SET);
      if (!Read_bytes(file, buffer, 8))
        return;
      if (strncmp(buffer, "PAL data", 8) == 0)
      {
        File_error = 0;
      }
    }
  }
}

/// Test for GPL (Gimp Palette) file format
void Test_GPL(T_IO_Context * context, FILE * file)
{
  char buffer[16];
  long file_size;

  (void)context;
  File_error = 1;

  file_size = File_length_file(file);
  if (file_size > 33) {
    // minimum header length == 33
    // "GIMP Palette" == 12
    if (!Read_bytes(file, buffer, 12))
      return;
    if (strncmp(buffer,"GIMP Palette",12) == 0)
      File_error = 0;
  }
}

/// skip the padding before a space-padded field.
static int skip_padding(FILE *file, int max_chars)
{
  byte b;
  int chars_read = 0;

  do {
    if (chars_read == max_chars)
      return chars_read; // eof
    if (!Read_byte(file, &b))
      return chars_read;
    chars_read++;
  } while (b == ' ');

  fseek(file, -1, SEEK_CUR);
  return chars_read;
}

/// Load GPL (Gimp Palette) file format
void Load_GPL(T_IO_Context * context)
{
  FILE *file;
  char buffer[256];

  File_error = 1;

  file = Open_file_read(context);
  if (file == NULL)
    return;

  if (!Read_byte_line(file, buffer, sizeof(buffer)))
    return;

  if (memcmp(buffer,"GIMP Palette",12) == 0)
  {
    int i, r, g, b, columns;
    size_t len;
    // Name: xxxxx
    if (!Read_byte_line(file, buffer, sizeof(buffer)))
      return;
    len = strlen(buffer);
    while (len > 0)
    {
      len--;
      if (buffer[len] == '\r' || buffer[len] == '\n')
        buffer[len] = '\0';
    }
    GFX2_Log(GFX2_DEBUG, "GPL %s\n", buffer);
    if (0 == memcmp(buffer, "Name: ", 6))
      snprintf(context->Comment, sizeof(context->Comment), "GPL: %s", buffer + 6);

    // Columns: 16
    if (fscanf(file, "Columns: %d", &columns) != 1)
      return;
    Read_byte_line(file, buffer, sizeof(buffer));
    /// @todo set grafx2 columns setting to match.
    // #<newline>

    for (i = 0; i < 256; i++)
    {
      for (;;)
      {
        // skip comments
        int c = getc(file);
        if (c == '#')
        {
          if (!Read_byte_line(file, buffer, sizeof(buffer)))
            return;
          GFX2_Log(GFX2_DEBUG, "comment: %s", buffer);
        }
        else
        {
          fseek(file, -1, SEEK_CUR);
          break;
        }
      }
      skip_padding(file, 32);
      if (fscanf(file, "%d", &r) != 1)
        break;
      skip_padding(file, 32);
      if (fscanf(file, "%d", &g) != 1)
        break;
      skip_padding(file, 32);
      if (fscanf(file, "%d\t", &b) != 1)
        break;
      if (!Read_byte_line(file, buffer, sizeof(buffer)))
        break;
      len = strlen(buffer);
      while (len > 1)
      {
        len--;
        if (buffer[len] == '\r' || buffer[len] == '\n')
          buffer[len] = '\0';
      }
      /// @todo analyze color names to build shade table

      GFX2_Log(GFX2_DEBUG, "GPL: %3d: RGB(%3d,%3d,%3d) %s\n", i, r,g,b, buffer);
      context->Palette[i].R = r;
      context->Palette[i].G = g;
      context->Palette[i].B = b;
    }
    if (i > 0)  // at least one color was read
      File_error = 0;
  }
  else
    File_error = 2;

  // close the file
  fclose(file);
}


/// Save GPL (Gimp Palette) file format
void
Save_GPL (T_IO_Context * context)
{
  // Gimp is a unix program, so use Unix file endings (LF aka '\n')
  FILE *file;

  file = Open_file_write(context);

  if (file != NULL )
  {
    int i;

    File_error = 0;
    fprintf (file, "GIMP Palette\n");
    fprintf (file, "Name: %s\n", context->File_name);
    // TODO: use actual columns value
    fprintf (file, "Columns: %d\n#\n", 16);

    for (i = 0; i < 256 && File_error==0; i++)
    {
      // TODO: build names from shade table data
      if (fprintf(file,"%d %d %d\tUntitled\n",context->Palette[i].R, context->Palette[i].G, context->Palette[i].B) <= 0)
        File_error=1;
    }
    fclose(file);

    if (File_error)
      Remove_file(context);
  }
  else
  {
    // unable to open output file, nothing saved.
    File_error=1;
  }
}



// -- Lire un fichier au format PAL -----------------------------------------
void Load_PAL(T_IO_Context * context)
{
  FILE *file;              // Fichier du fichier

  File_error=0;

  // Ouverture du fichier
  if ((file=Open_file_read(context)))
  {
    long file_size = File_length_file(file);
    // Le fichier ne peut être au format PAL que si sa taille vaut 768 octets
    if (file_size == sizeof(T_Palette))
    {
      T_Palette palette_64;
      // Pre_load(context, ?); // Pas possible... pas d'image...

      // Lecture du fichier dans context->Palette
      if (Read_bytes(file, palette_64, sizeof(T_Palette)))
      {
        Palette_64_to_256(palette_64);
        memcpy(context->Palette, palette_64, sizeof(T_Palette));
      }
      else
        File_error = 2;
    } else {
      char buffer[16];
      if (!Read_bytes(file, buffer, 8))
      {
        File_error = 2;
        fclose(file);
        return;
      }
      buffer[8] = '\0';
      if (strncmp(buffer,"JASC-PAL",8) == 0)
      {
        int i, n, r, g, b;
        i = fscanf(file, "%d",&n);
        if(i != 1 || n != 100)
        {
          File_error = 2;
          fclose(file);
          return;
        }
        // Read color count
        if (fscanf(file, "%d",&n) == 1)
        {
          for (i = 0; i < n; i++)
          {
            if (fscanf(file, "%d %d %d",&r, &g, &b) == 3)
            {
              context->Palette[i].R = r;
              context->Palette[i].G = g;
              context->Palette[i].B = b;
            }
            else
              File_error = 2;
          }
        }
        else
          File_error = 2;
      }
      else if(strncmp(buffer, "RIFF", 4) == 0)
      {
        // Microsoft RIFF format.
        fseek(file, 8, SEEK_SET);
        Read_bytes(file, buffer, 8);
        if (strncmp(buffer, "PAL data", 8) == 0)
        {
          word color_count;
          word i = 0;

          fseek(file, 22, SEEK_SET);
          if (!Read_word_le(file, &color_count))
            File_error = 2;
          else
            for(i = 0; i < color_count && File_error == 0; i++)
            {
              byte colors[4];
              if (!Read_bytes(file, colors, 4))
                File_error = 2;
              context->Palette[i].R = colors[0];
              context->Palette[i].G = colors[1];
              context->Palette[i].B = colors[2];
            }
        } else File_error = 2;
      } else
        File_error = 2;
    }

    fclose(file);
  }
  else
    // Si on n'a pas réussi à ouvrir le fichier, alors il y a eu une erreur
    File_error=1;
}


// -- Sauver un fichier au format PAL ---------------------------------------
void Save_PAL(T_IO_Context * context)
{
  // JASC-PAL is a DOS/Windows format, so use CRLF line endings "\r\n"
  FILE *file;

  File_error=0;

  // Open output file
  if ((file=Open_file_write(context)) != NULL)
  {
    int i;

    setvbuf(file, NULL, _IOFBF, 64*1024);

    if (fputs("JASC-PAL\r\n0100\r\n256\r\n", file)==EOF)
      File_error=1;
    for (i = 0; i < 256 && File_error==0; i++)
    {
      if (fprintf(file,"%d %d %d\r\n",context->Palette[i].R, context->Palette[i].G, context->Palette[i].B) <= 0)
        File_error=1;
    }

    fclose(file);

    if (File_error)
      Remove_file(context);
  }
  else
  {
    // unable to open output file, nothing saved.
    File_error=1;
  }
}


//////////////////////////////////// PKM ////////////////////////////////////
typedef struct
{
  char Ident[3];    // String "PKM" }
  byte Method;      // Compression method
                    //   0 = per-line compression (c)KM
                    //   others = unknown at the moment
  byte Recog1;      // Recognition byte 1
  byte Recog2;      // Recognition byte 2
  word Width;       // Image width
  word Height;      // Image height
  T_Palette Palette;// RGB Palette 256*3, on a 1-64 scale for each component
  word Jump;        // Size of the jump between header and image:
                    //   Used to insert a comment
} T_PKM_Header;

// -- Tester si un fichier est au format PKM --------------------------------
void Test_PKM(T_IO_Context * context, FILE * file)
{
  T_PKM_Header header;

  (void)context;
  File_error=1;

  // Lecture du header du fichier
  if (Read_bytes(file,&header.Ident,3) &&
      Read_byte(file,&header.Method) &&
      Read_byte(file,&header.Recog1) &&
      Read_byte(file,&header.Recog2) &&
      Read_word_le(file,&header.Width) &&
      Read_word_le(file,&header.Height) &&
      Read_bytes(file,&header.Palette,sizeof(T_Palette)) &&
      Read_word_le(file,&header.Jump))
  {
    // On regarde s'il y a la signature PKM suivie de la méthode 0.
    // La constante "PKM" étant un chaîne, elle se termine toujours par 0.
    // Donc pas la peine de s'emm...er à regarder si la méthode est à 0.
    if ( (!memcmp(&header,"PKM",4)) && header.Width && header.Height)
      File_error=0;
  }
}


// -- Lire un fichier au format PKM -----------------------------------------
void Load_PKM(T_IO_Context * context)
{
  FILE *file;             // Fichier du fichier
  T_PKM_Header header;
  byte  color;
  byte  temp_byte;
  word  len;
  word  index;
  dword Compteur_de_pixels;
  dword Compteur_de_donnees_packees;
  dword image_size;
  dword Taille_pack;
  long  file_size;

  File_error=0;

  if ((file=Open_file_read(context)))
  {
    file_size=File_length_file(file);

    if (Read_bytes(file,&header.Ident,3) &&
        Read_byte(file,&header.Method) &&
        Read_byte(file,&header.Recog1) &&
        Read_byte(file,&header.Recog2) &&
        Read_word_le(file,&header.Width) &&
        Read_word_le(file,&header.Height) &&
        Read_bytes(file,&header.Palette,sizeof(T_Palette)) &&
        Read_word_le(file,&header.Jump))
    {
      context->Comment[0]='\0'; // On efface le commentaire
      if (header.Jump)
      {
        index=0;
        while ( (index<header.Jump) && (!File_error) )
        {
          if (Read_byte(file,&temp_byte))
          {
            index+=2; // On rajoute le "Field-id" et "le Field-size" pas encore lu
            switch (temp_byte)
            {
              case 0 : // Commentaire
                if (Read_byte(file,&temp_byte))
                {
                  if (temp_byte>COMMENT_SIZE)
                  {
                    color=temp_byte;              // On se sert de color comme
                    temp_byte=COMMENT_SIZE;   // variable temporaire
                    color-=COMMENT_SIZE;
                  }
                  else
                    color=0;

                  if (Read_bytes(file,context->Comment,temp_byte))
                  {
                    index+=temp_byte;
                    context->Comment[temp_byte]='\0';
                    if (color)
                      if (fseek(file,color,SEEK_CUR))
                        File_error=2;
                  }
                  else
                    File_error=2;
                }
                else
                  File_error=2;
                break;

              case 1 : // Dimensions de l'écran d'origine
                if (Read_byte(file,&temp_byte))
                {
                  if (temp_byte==4)
                  {
                    index+=4;
                    if ( ! Read_word_le(file,(word *) &Original_screen_X)
                      || !Read_word_le(file,(word *) &Original_screen_Y) )
                      File_error=2;
                    else
                      GFX2_Log(GFX2_DEBUG, "PKM original screen %dx%d\n", (int)Original_screen_X, (int)Original_screen_Y);
                  }
                  else
                    File_error=2;
                }
                else
                  File_error=2;
                break;

              case 2 : // color de transparence
                if (Read_byte(file,&temp_byte))
                {
                  if (temp_byte==1)
                  {
                    index++;
                    if (! Read_byte(file,&Back_color))
                      File_error=2;
                  }
                  else
                    File_error=2;
                }
                else
                  File_error=2;
                break;

              default:
                if (Read_byte(file,&temp_byte))
                {
                  index+=temp_byte;
                  if (fseek(file,temp_byte,SEEK_CUR))
                    File_error=2;
                }
                else
                  File_error=2;
            }
          }
          else
            File_error=2;
        }
        if ( (!File_error) && (index!=header.Jump) )
          File_error=2;
      }

      /*Init_lecture();*/

      if (!File_error)
      {
        Pre_load(context, header.Width,header.Height,file_size,FORMAT_PKM,PIXEL_SIMPLE,0);
        if (File_error==0)
        {

          context->Width=header.Width;
          context->Height=header.Height;
          image_size=(dword)(context->Width*context->Height);
          // Palette lue en 64
          memcpy(context->Palette,header.Palette,sizeof(T_Palette));
          Palette_64_to_256(context->Palette);

          Compteur_de_donnees_packees=0;
          Compteur_de_pixels=0;
          // Header size is 780
          Taille_pack=(file_size)-780-header.Jump;

          // Boucle de décompression:
          while ( (Compteur_de_pixels<image_size) && (Compteur_de_donnees_packees<Taille_pack) && (!File_error) )
          {
            if(Read_byte(file, &temp_byte)!=1)
            {
              File_error=2;
              break;
            }

            // Si ce n'est pas un octet de reconnaissance, c'est un pixel brut
            if ( (temp_byte!=header.Recog1) && (temp_byte!=header.Recog2) )
            {
              Set_pixel(context, Compteur_de_pixels % context->Width,
                                  Compteur_de_pixels / context->Width,
                                  temp_byte);
              Compteur_de_donnees_packees++;
              Compteur_de_pixels++;
            }
            else // Sinon, On regarde si on va décompacter un...
            { // ... nombre de pixels tenant sur un byte
                if (temp_byte==header.Recog1)
                {
                  if(Read_byte(file, &color)!=1)
                {
                    File_error=2;
                    break;
                }
                if(Read_byte(file, &temp_byte)!=1)
                {
                    File_error=2;
                    break;
                }
                for (index=0; index<temp_byte; index++)
                  Set_pixel(context, (Compteur_de_pixels+index) % context->Width,
                                      (Compteur_de_pixels+index) / context->Width,
                                      color);
                Compteur_de_pixels+=temp_byte;
                Compteur_de_donnees_packees+=3;
              }
              else // ... nombre de pixels tenant sur un word
              {
                if(Read_byte(file, &color)!=1)
                {
                    File_error=2;
                    break;
        }
                Read_word_be(file, &len);
                for (index=0; index<len; index++)
                  Set_pixel(context, (Compteur_de_pixels+index) % context->Width,
                                      (Compteur_de_pixels+index) / context->Width,
                                      color);
                Compteur_de_pixels+=len;
                Compteur_de_donnees_packees+=4;
              }
            }
          }
        }
      }
      /*Close_lecture();*/
    }
    else // Lecture header impossible: Error ne modifiant pas l'image
      File_error=1;

    fclose(file);
  }
  else // Ouv. fichier impossible: Error ne modifiant pas l'image
    File_error=1;
}


// -- Sauver un fichier au format PKM ---------------------------------------

  // Trouver quels sont les octets de reconnaissance
  void Find_recog(byte * recog1, byte * recog2)
  {
    dword Find_recon[256]; // Table d'utilisation de couleurs
    byte  best;   // Meilleure couleur pour recon (recon1 puis recon2)
    dword NBest;  // Nombre d'occurences de cette couleur
    word  index;


    // On commence par compter l'utilisation de chaque couleurs
    Count_used_colors(Find_recon);

    // Ensuite recog1 devient celle la moins utilisée de celles-ci
    *recog1=0;
    best=1;
    NBest=INT_MAX; // Une même couleur ne pourra jamais être utilisée 1M de fois.
    for (index=1;index<=255;index++)
      if (Find_recon[index]<NBest)
      {
        best=index;
        NBest=Find_recon[index];
      }
    *recog1=best;

    // Enfin recog2 devient la 2ème moins utilisée
    *recog2=0;
    best=0;
    NBest=INT_MAX;
    for (index=0;index<=255;index++)
      if ( (Find_recon[index]<NBest) && (index!=*recog1) )
      {
        best=index;
        NBest=Find_recon[index];
      }
    *recog2=best;
  }


void Save_PKM(T_IO_Context * context)
{
  FILE *file;
  T_PKM_Header header;
  dword Compteur_de_pixels;
  dword image_size;
  word  repetitions;
  byte  last_color;
  byte  pixel_value;
  size_t comment_size;



  // Construction du header
  memcpy(header.Ident,"PKM",3);
  header.Method=0;
  Find_recog(&header.Recog1,&header.Recog2);
  header.Width=context->Width;
  header.Height=context->Height;
  memcpy(header.Palette,context->Palette,sizeof(T_Palette));
  Palette_256_to_64(header.Palette);

  // Calcul de la taille du Post-header
  header.Jump=9; // 6 pour les dimensions de l'ecran + 3 pour la back-color
  comment_size=strlen(context->Comment);
  if (comment_size > 255) comment_size = 255;
  if (comment_size)
    header.Jump+=(word)comment_size+2;


  File_error=0;

  // Ouverture du fichier
  if ((file=Open_file_write(context)))
  {
    setvbuf(file, NULL, _IOFBF, 64*1024);

    // Ecriture du header
    if (Write_bytes(file,&header.Ident,3) &&
        Write_byte(file,header.Method) &&
        Write_byte(file,header.Recog1) &&
        Write_byte(file,header.Recog2) &&
        Write_word_le(file,header.Width) &&
        Write_word_le(file,header.Height) &&
        Write_bytes(file,&header.Palette,sizeof(T_Palette)) &&
        Write_word_le(file,header.Jump))
    {

      // Ecriture du commentaire
      // (Compteur_de_pixels est utilisé ici comme simple index de comptage)
      if (comment_size > 0)
      {
        Write_one_byte(file,0);
        Write_one_byte(file,(byte)comment_size);
        for (Compteur_de_pixels=0; Compteur_de_pixels<comment_size; Compteur_de_pixels++)
          Write_one_byte(file,context->Comment[Compteur_de_pixels]);
      }
      // Ecriture des dimensions de l'écran
      Write_one_byte(file,1);
      Write_one_byte(file,4);
      Write_one_byte(file,Screen_width&0xFF);
      Write_one_byte(file,Screen_width>>8);
      Write_one_byte(file,Screen_height&0xFF);
      Write_one_byte(file,Screen_height>>8);
      // Ecriture de la back-color
      Write_one_byte(file,2);
      Write_one_byte(file,1);
      Write_one_byte(file,Back_color);

      // Routine de compression PKM de l'image
      image_size=(dword)(context->Width*context->Height);
      Compteur_de_pixels=0;
      pixel_value=Get_pixel(context, 0,0);

      while ( (Compteur_de_pixels<image_size) && (!File_error) )
      {
        Compteur_de_pixels++;
        repetitions=1;
        last_color=pixel_value;
        if(Compteur_de_pixels<image_size)
        {
          pixel_value=Get_pixel(context, Compteur_de_pixels % context->Width,Compteur_de_pixels / context->Width);
        }
        while ( (pixel_value==last_color)
             && (Compteur_de_pixels<image_size)
             && (repetitions<65535) )
        {
          Compteur_de_pixels++;
          repetitions++;
          if(Compteur_de_pixels>=image_size) break;
          pixel_value=Get_pixel(context, Compteur_de_pixels % context->Width,Compteur_de_pixels / context->Width);
        }

        if ( (last_color!=header.Recog1) && (last_color!=header.Recog2) )
        {
          if (repetitions==1)
            Write_one_byte(file,last_color);
          else
          if (repetitions==2)
          {
            Write_one_byte(file,last_color);
            Write_one_byte(file,last_color);
          }
          else
          if ( (repetitions>2) && (repetitions<256) )
          { // RECON1/couleur/nombre
            Write_one_byte(file,header.Recog1);
            Write_one_byte(file,last_color);
            Write_one_byte(file,repetitions&0xFF);
          }
          else
          if (repetitions>=256)
          { // RECON2/couleur/hi(nombre)/lo(nombre)
            Write_one_byte(file,header.Recog2);
            Write_one_byte(file,last_color);
            Write_one_byte(file,repetitions>>8);
            Write_one_byte(file,repetitions&0xFF);
          }
        }
        else
        {
          if (repetitions<256)
          {
            Write_one_byte(file,header.Recog1);
            Write_one_byte(file,last_color);
            Write_one_byte(file,repetitions&0xFF);
          }
          else
          {
            Write_one_byte(file,header.Recog2);
            Write_one_byte(file,last_color);
            Write_one_byte(file,repetitions>>8);
            Write_one_byte(file,repetitions&0xFF);
          }
        }
      }
    }
    else
      File_error=1;
    fclose(file);
  }
  else
  {
    File_error=1;
    fclose(file);
  }
  //   S'il y a eu une erreur de sauvegarde, on ne va tout de même pas laisser
  // ce fichier pourri traîner... Ca fait pas propre.
  if (File_error)
    Remove_file(context);
}


//////////////////////////////////// CEL ////////////////////////////////////
typedef struct
{
  word Width;              // width de l'image
  word Height;             // height de l'image
} T_CEL_Header1;

typedef struct
{
  byte Signature[4];           // Signature du format
  byte Kind;               // Type de fichier ($10=PALette $20=BitMaP)
  byte Nb_bits;             // Nombre de bits
  word Filler1;            // ???
  word Width;            // width de l'image
  word Height;            // height de l'image
  word X_offset;         // Offset en X de l'image
  word Y_offset;         // Offset en Y de l'image
  byte Filler2[16];        // ???
} T_CEL_Header2;

// -- Tester si un fichier est au format CEL --------------------------------

void Test_CEL(T_IO_Context * context, FILE * file)
{
  int  size;
  T_CEL_Header1 header1;
  T_CEL_Header2 header2;
  int file_size;

  (void)context;
  File_error=0;

  file_size = File_length_file(file);
  if (Read_word_le(file,&header1.Width) &&
      Read_word_le(file,&header1.Height) )
  {
      //   Vu que ce header n'a pas de signature, il va falloir tester la
      // cohérence de la dimension de l'image avec celle du fichier.

      size=file_size-4;
      if ( (!size) || ( (((header1.Width+1)>>1)*header1.Height)!=size ) )
      {
        // Tentative de reconnaissance de la signature des nouveaux fichiers

        fseek(file,0,SEEK_SET);
        if (Read_bytes(file,&header2.Signature,4) &&
            !memcmp(header2.Signature,"KiSS",4) &&
            Read_byte(file,&header2.Kind) &&
            (header2.Kind==0x20) &&
            Read_byte(file,&header2.Nb_bits) &&
            Read_word_le(file,&header2.Filler1) &&
            Read_word_le(file,&header2.Width) &&
            Read_word_le(file,&header2.Height) &&
            Read_word_le(file,&header2.X_offset) &&
            Read_word_le(file,&header2.Y_offset) &&
            Read_bytes(file,&header2.Filler2,16))
        {
          // ok
        }
        else
          File_error=1;
      }
      else
        File_error=1;
  }
  else
  {
    File_error=1;
  }
}


// -- Lire un fichier au format CEL -----------------------------------------

void Load_CEL(T_IO_Context * context)
{
  FILE *file;
  T_CEL_Header1 header1;
  T_CEL_Header2 header2;
  short x_pos;
  short y_pos;
  byte  last_byte=0;
  long  file_size;
  const long int header_size = 4;

  File_error=0;
  if ((file=Open_file_read(context)))
  {
    if (Read_word_le(file,&(header1.Width))
    &&  Read_word_le(file,&(header1.Height)))
    {
      file_size=File_length_file(file);
      if ( (file_size>header_size)
        && ( (((header1.Width+1)>>1)*header1.Height)==(file_size-header_size) ) )
      {
        // Chargement d'un fichier CEL sans signature (vieux fichiers)
        context->Width=header1.Width;
        context->Height=header1.Height;
        Original_screen_X=context->Width;
        Original_screen_Y=context->Height;
        Pre_load(context, context->Width,context->Height,file_size,FORMAT_CEL,PIXEL_SIMPLE,0);
        if (File_error==0)
        {
          // Chargement de l'image
          /*Init_lecture();*/
          for (y_pos=0;((y_pos<context->Height) && (!File_error));y_pos++)
            for (x_pos=0;((x_pos<context->Width) && (!File_error));x_pos++)
              if ((x_pos & 1)==0)
              {
                if(Read_byte(file,&last_byte)!=1) File_error = 2;
                Set_pixel(context, x_pos,y_pos,(last_byte >> 4));
              }
              else
                Set_pixel(context, x_pos,y_pos,(last_byte & 15));
          /*Close_lecture();*/
        }
      }
      else
      {
        // On réessaye avec le nouveau format

        fseek(file,0,SEEK_SET);
        if (Read_bytes(file,header2.Signature,4)
        && Read_byte(file,&(header2.Kind))
        && Read_byte(file,&(header2.Nb_bits))
        && Read_word_le(file,&(header2.Filler1))
        && Read_word_le(file,&(header2.Width))
        && Read_word_le(file,&(header2.Height))
        && Read_word_le(file,&(header2.X_offset))
        && Read_word_le(file,&(header2.Y_offset))
        && Read_bytes(file,header2.Filler2,16)
        )
        {
          // Chargement d'un fichier CEL avec signature (nouveaux fichiers)

          context->Width=header2.Width+header2.X_offset;
          context->Height=header2.Height+header2.Y_offset;
          Original_screen_X=context->Width;
          Original_screen_Y=context->Height;
          Pre_load(context, context->Width,context->Height,file_size,FORMAT_CEL,PIXEL_SIMPLE,0);
          if (File_error==0)
          {
            // Chargement de l'image
            /*Init_lecture();*/

            if (!File_error)
            {
              // Effacement du décalage
              for (y_pos=0;y_pos<header2.Y_offset;y_pos++)
                for (x_pos=0;x_pos<context->Width;x_pos++)
                  Set_pixel(context, x_pos,y_pos,0);
              for (y_pos=header2.Y_offset;y_pos<context->Height;y_pos++)
                for (x_pos=0;x_pos<header2.X_offset;x_pos++)
                  Set_pixel(context, x_pos,y_pos,0);

              switch(header2.Nb_bits)
              {
                case 4:
                  for (y_pos=0;((y_pos<header2.Height) && (!File_error));y_pos++)
                    for (x_pos=0;((x_pos<header2.Width) && (!File_error));x_pos++)
                      if ((x_pos & 1)==0)
                      {
                        if(Read_byte(file,&last_byte)!=1) File_error=2;
                        Set_pixel(context, x_pos+header2.X_offset,y_pos+header2.Y_offset,(last_byte >> 4));
                      }
                      else
                        Set_pixel(context, x_pos+header2.X_offset,y_pos+header2.Y_offset,(last_byte & 15));
                  break;

                case 8:
                  for (y_pos=0;((y_pos<header2.Height) && (!File_error));y_pos++)
                    for (x_pos=0;((x_pos<header2.Width) && (!File_error));x_pos++)
                    {
                      byte byte_read;
                      if(Read_byte(file,&byte_read)!=1) File_error = 2;
                      Set_pixel(context, x_pos+header2.X_offset,y_pos+header2.Y_offset,byte_read);
                      }
                  break;

                default:
                  File_error=1;
              }
            }
            /*Close_lecture();*/
          }
        }
        else
          File_error=1;
      }
      fclose(file);
    }
    else
      File_error=1;
  }
  else
    File_error=1;
}


// -- Ecrire un fichier au format CEL ---------------------------------------

void Save_CEL(T_IO_Context * context)
{
  FILE *file;
  T_CEL_Header1 header1;
  T_CEL_Header2 header2;
  short x_pos;
  short y_pos;
  byte  last_byte=0;
  dword color_usage[256]; // Table d'utilisation de couleurs


  // On commence par compter l'utilisation de chaque couleurs
  Count_used_colors(color_usage);

  File_error=0;
  if ((file=Open_file_write(context)))
  {
    setvbuf(file, NULL, _IOFBF, 64*1024);

    // On regarde si des couleurs >16 sont utilisées dans l'image
    for (x_pos=16;((x_pos<256) && (!color_usage[x_pos]));x_pos++);

    if (x_pos==256)
    {
      // Cas d'une image 16 couleurs (écriture à l'ancien format)

      header1.Width =context->Width;
      header1.Height=context->Height;

      if (Write_word_le(file,header1.Width)
      && Write_word_le(file,header1.Height)
      )
      {
        // Sauvegarde de l'image
        for (y_pos=0;((y_pos<context->Height) && (!File_error));y_pos++)
        {
          for (x_pos=0;((x_pos<context->Width) && (!File_error));x_pos++)
            if ((x_pos & 1)==0)
              last_byte=(Get_pixel(context, x_pos,y_pos) << 4);
            else
            {
              last_byte=last_byte | (Get_pixel(context, x_pos,y_pos) & 15);
              Write_one_byte(file,last_byte);
            }

          if ((x_pos & 1)==1)
            Write_one_byte(file,last_byte);
        }
      }
      else
        File_error=1;
      fclose(file);
    }
    else
    {
      // Cas d'une image 256 couleurs (écriture au nouveau format)

      // Recherche du décalage
      for (y_pos=0;y_pos<context->Height;y_pos++)
      {
        for (x_pos=0;x_pos<context->Width;x_pos++)
          if (Get_pixel(context, x_pos,y_pos)!=0)
            break;
        if (Get_pixel(context, x_pos,y_pos)!=0)
          break;
      }
      header2.Y_offset=y_pos;
      for (x_pos=0;x_pos<context->Width;x_pos++)
      {
        for (y_pos=0;y_pos<context->Height;y_pos++)
          if (Get_pixel(context, x_pos,y_pos)!=0)
            break;
        if (Get_pixel(context, x_pos,y_pos)!=0)
          break;
      }
      header2.X_offset=x_pos;

      memcpy(header2.Signature,"KiSS",4); // Initialisation de la signature
      header2.Kind=0x20;              // Initialisation du type (BitMaP)
      header2.Nb_bits=8;               // Initialisation du nombre de bits
      header2.Filler1=0;              // Initialisation du filler 1 (?)
      header2.Width=context->Width-header2.X_offset; // Initialisation de la largeur
      header2.Height=context->Height-header2.Y_offset; // Initialisation de la hauteur
      for (x_pos=0;x_pos<16;x_pos++)  // Initialisation du filler 2 (?)
        header2.Filler2[x_pos]=0;

      if (Write_bytes(file,header2.Signature,4)
      && Write_byte(file,header2.Kind)
      && Write_byte(file,header2.Nb_bits)
      && Write_word_le(file,header2.Filler1)
      && Write_word_le(file,header2.Width)
      && Write_word_le(file,header2.Height)
      && Write_word_le(file,header2.X_offset)
      && Write_word_le(file,header2.Y_offset)
      && Write_bytes(file,header2.Filler2,14)
      )
      {
        // Sauvegarde de l'image
        for (y_pos=0;((y_pos<header2.Height) && (!File_error));y_pos++)
          for (x_pos=0;((x_pos<header2.Width) && (!File_error));x_pos++)
            Write_one_byte(file,Get_pixel(context, x_pos+header2.X_offset,y_pos+header2.Y_offset));
      }
      else
        File_error=1;
      fclose(file);
    }

    if (File_error)
      Remove_file(context);
  }
  else
    File_error=1;
}


//////////////////////////////////// KCF ////////////////////////////////////
typedef struct
{
  struct
  {
    struct
    {
      byte Byte1;
      byte Byte2;
    } color[16];
  } Palette[10];
} T_KCF_Header;

// -- Tester si un fichier est au format KCF --------------------------------

void Test_KCF(T_IO_Context * context, FILE * file)
{
  T_KCF_Header header1;
  T_CEL_Header2 header2;
  int pal_index;
  int color_index;

  (void)context;
  File_error=0;
    if (File_length_file(file)==320)
    {
      for (pal_index=0;pal_index<10 && !File_error;pal_index++)
        for (color_index=0;color_index<16 && !File_error;color_index++)
          if (!Read_byte(file,&header1.Palette[pal_index].color[color_index].Byte1) ||
              !Read_byte(file,&header1.Palette[pal_index].color[color_index].Byte2))
            File_error=1;
      // On vérifie une propriété de la structure de palette:
      for (pal_index=0;pal_index<10;pal_index++)
        for (color_index=0;color_index<16;color_index++)
          if ((header1.Palette[pal_index].color[color_index].Byte2>>4)!=0)
            File_error=1;
    }
    else
    {
      if (Read_bytes(file,header2.Signature,4)
        && Read_byte(file,&(header2.Kind))
        && Read_byte(file,&(header2.Nb_bits))
        && Read_word_le(file,&(header2.Filler1))
        && Read_word_le(file,&(header2.Width))
        && Read_word_le(file,&(header2.Height))
        && Read_word_le(file,&(header2.X_offset))
        && Read_word_le(file,&(header2.Y_offset))
        && Read_bytes(file,header2.Filler2,14)
        )
      {
        if (memcmp(header2.Signature,"KiSS",4)==0)
        {
          if (header2.Kind!=0x10)
            File_error=1;
        }
        else
          File_error=1;
      }
      else
        File_error=1;
    }
}


// -- Lire un fichier au format KCF -----------------------------------------

void Load_KCF(T_IO_Context * context)
{
  FILE *file;
  T_KCF_Header header1;
  T_CEL_Header2 header2;
  byte bytes[3];
  int pal_index;
  int color_index;
  int index;
  long  file_size;


  File_error=0;
  if ((file=Open_file_read(context)))
  {
    file_size=File_length_file(file);
    if (file_size==320)
    {
      // Fichier KCF à l'ancien format
      for (pal_index=0;pal_index<10 && !File_error;pal_index++)
        for (color_index=0;color_index<16 && !File_error;color_index++)
          if (!Read_byte(file,&header1.Palette[pal_index].color[color_index].Byte1) ||
              !Read_byte(file,&header1.Palette[pal_index].color[color_index].Byte2))
            File_error=1;

      if (!File_error)
      {
        // Pre_load(context, ?); // Pas possible... pas d'image...

        if (Config.Clear_palette)
          memset(context->Palette,0,sizeof(T_Palette));

        // Chargement de la palette
        for (pal_index=0;pal_index<10;pal_index++)
          for (color_index=0;color_index<16;color_index++)
          {
            index=16+(pal_index*16)+color_index;
            context->Palette[index].R=((header1.Palette[pal_index].color[color_index].Byte1 >> 4) << 4);
            context->Palette[index].B=((header1.Palette[pal_index].color[color_index].Byte1 & 15) << 4);
            context->Palette[index].G=((header1.Palette[pal_index].color[color_index].Byte2 & 15) << 4);
          }

        for (index=0;index<16;index++)
        {
          context->Palette[index].R=context->Palette[index+16].R;
          context->Palette[index].G=context->Palette[index+16].G;
          context->Palette[index].B=context->Palette[index+16].B;
        }

      }
      else
        File_error=1;
    }
    else
    {
      // Fichier KCF au nouveau format

      if (Read_bytes(file,header2.Signature,4)
        && Read_byte(file,&(header2.Kind))
        && Read_byte(file,&(header2.Nb_bits))
        && Read_word_le(file,&(header2.Filler1))
        && Read_word_le(file,&(header2.Width))
        && Read_word_le(file,&(header2.Height))
        && Read_word_le(file,&(header2.X_offset))
        && Read_word_le(file,&(header2.Y_offset))
        && Read_bytes(file,header2.Filler2,14)
        )
      {
        // Pre_load(context, ?); // Pas possible... pas d'image...

        index=(header2.Nb_bits==12)?16:0;
        for (pal_index=0;pal_index<header2.Height;pal_index++)
        {
           // Pour chaque palette

           for (color_index=0;color_index<header2.Width;color_index++)
           {
             // Pour chaque couleur

             switch(header2.Nb_bits)
             {
               case 12: // RRRR BBBB | 0000 VVVV
                 Read_bytes(file,bytes,2);
                 context->Palette[index].R=(bytes[0] >> 4) << 4;
                 context->Palette[index].B=(bytes[0] & 15) << 4;
                 context->Palette[index].G=(bytes[1] & 15) << 4;
                 break;

               case 24: // RRRR RRRR | VVVV VVVV | BBBB BBBB
                 Read_bytes(file,bytes,3);
                 context->Palette[index].R=bytes[0];
                 context->Palette[index].G=bytes[1];
                 context->Palette[index].B=bytes[2];
             }

             index++;
           }
        }

        if (header2.Nb_bits==12)
          for (index=0;index<16;index++)
          {
            context->Palette[index].R=context->Palette[index+16].R;
            context->Palette[index].G=context->Palette[index+16].G;
            context->Palette[index].B=context->Palette[index+16].B;
          }

      }
      else
        File_error=1;
    }
    fclose(file);
  }
  else
    File_error=1;
}


// -- Ecrire un fichier au format KCF ---------------------------------------

void Save_KCF(T_IO_Context * context)
{
  FILE *file;
  T_KCF_Header header1;
  T_CEL_Header2 header2;
  byte bytes[3];
  int pal_index;
  int color_index;
  int index;
  dword color_usage[256]; // Table d'utilisation de couleurs

  // On commence par compter l'utilisation de chaque couleurs
  Count_used_colors(color_usage);

  File_error=0;
  if ((file=Open_file_write(context)))
  {
    setvbuf(file, NULL, _IOFBF, 64*1024);
    // Sauvegarde de la palette

    // On regarde si des couleurs >16 sont utilisées dans l'image
    for (index=16;((index<256) && (!color_usage[index]));index++);

    if (index==256)
    {
      // Cas d'une image 16 couleurs (écriture à l'ancien format)

      for (pal_index=0;pal_index<10;pal_index++)
        for (color_index=0;color_index<16;color_index++)
        {
          index=16+(pal_index*16)+color_index;
          header1.Palette[pal_index].color[color_index].Byte1=((context->Palette[index].R>>4)<<4) | (context->Palette[index].B>>4);
          header1.Palette[pal_index].color[color_index].Byte2=context->Palette[index].G>>4;
        }

      // Write all
      for (pal_index=0;pal_index<10 && !File_error;pal_index++)
        for (color_index=0;color_index<16 && !File_error;color_index++)
          if (!Write_byte(file,header1.Palette[pal_index].color[color_index].Byte1) ||
              !Write_byte(file,header1.Palette[pal_index].color[color_index].Byte2))
            File_error=1;
    }
    else
    {
      // Cas d'une image 256 couleurs (écriture au nouveau format)

      memcpy(header2.Signature,"KiSS",4); // Initialisation de la signature
      header2.Kind=0x10;              // Initialisation du type (PALette)
      header2.Nb_bits=24;              // Initialisation du nombre de bits
      header2.Filler1=0;              // Initialisation du filler 1 (?)
      header2.Width=256;            // Initialisation du nombre de couleurs
      header2.Height=1;              // Initialisation du nombre de palettes
      header2.X_offset=0;           // Initialisation du décalage X
      header2.Y_offset=0;           // Initialisation du décalage Y
      for (index=0;index<16;index++) // Initialisation du filler 2 (?)
        header2.Filler2[index]=0;

      if (!Write_bytes(file,header2.Signature,4)
      || !Write_byte(file,header2.Kind)
      || !Write_byte(file,header2.Nb_bits)
      || !Write_word_le(file,header2.Filler1)
      || !Write_word_le(file,header2.Width)
      || !Write_word_le(file,header2.Height)
      || !Write_word_le(file,header2.X_offset)
      || !Write_word_le(file,header2.Y_offset)
      || !Write_bytes(file,header2.Filler2,14)
      )
        File_error=1;

      for (index=0;(index<256) && (!File_error);index++)
      {
        bytes[0]=context->Palette[index].R;
        bytes[1]=context->Palette[index].G;
        bytes[2]=context->Palette[index].B;
        if (! Write_bytes(file,bytes,3))
          File_error=1;
      }
    }

    fclose(file);

    if (File_error)
      Remove_file(context);
  }
  else
    File_error=1;
}


//////////////////////////////////// C64 ////////////////////////////////////

/** C64 file formats
 */
enum c64_format
{
  F_invalid = -1,
  F_hires = 0,    ///< 320x200
  F_multi = 1,    ///< 160x200
  F_bitmap = 2,   ///< 320x200 monochrome
  F_fli = 3       ///< FLI (Flexible Line Interpretation)
};

/** C64 file formats names
 */
static const char *c64_format_names[] = {
  "Hires",
  "Multicolor",
  "Bitmap",
  "FLI"
};

static long C64_unpack_doodle(byte ** file_buffer, long file_size);

/**
 * Test for a C64 picture file
 *
 * Checks the file size and the load address
 *
 * References :
 * - http://unusedino.de/ec64/technical/formats/bitmap.html
 * - http://codebase64.org/doku.php?id=base:c64_grafix_files_specs_list_v0.03
 * - https://sourceforge.net/p/view64/code/HEAD/tree/trunk/libview64.c#l3737
 */
void Test_C64(T_IO_Context * context, FILE * file)
{
  unsigned long file_size;
  word load_addr;
  byte header[14];

  (void)context;
  File_error = 1;
  file_size = File_length_file(file);
  if (file_size < 16 || file_size > 48*1024)
    return; // File too short or too long, exit now
  // First test for formats without load address
  switch (file_size)
  {
    // case 1000: // screen or color
    case 8000: // raw bitmap
    case 9000: // bitmap + ScreenRAM
    case 10001: // multicolor
    case 17472: // FLI (BlackMail)
      File_error = 0;
      return;
    default: // then we don't know for now.
      if (!Read_word_le(file, &load_addr))
        return;
  }
  GFX2_Log(GFX2_DEBUG, "Test_C64() file_size=%ld LoadAddr=$%04X\n", file_size, load_addr);
  if (!Read_bytes(file, header, sizeof(header)))
    return;
  if (memcmp(header, "DRAZPAINT", 9) == 0)
  {
    GFX2_Log(GFX2_DEBUG, "Test_C64() header=%.13s RLE code = $%02X\n", header, header[13]);
    File_error = 0;
    return;
  }
  // check last 2 bytes
  if (fseek(file, -2, SEEK_END) < 0)
    return;
  if (!Read_bytes(file, header, 2))
    return;
  if (load_addr == 0x4000 && header[0] == 0xC2 && header[1] == 0x00) // Amica Paint EOF mark
  {
    File_error = 0;
    return;
  }
  switch (file_size)
  {
    // case 1002: // (screen or color) + loadaddr
    case 8002: // raw bitmap with loadaddr
    case 9002: // bitmap + ScreenRAM + loadaddr
      // $4000 => InterPaint Hi-Res (.iph)
    case 9003: // bitmap + ScreenRAM + loadaddr (+ border ?)
    case 9009: // bitmap + ScreenRAM + loadaddr
      // $2000 => Art Studio
    case 9218:
      // $5C00 => Doodle
    case 9332:
      // $3F8E => Paint Magic (.pmg) 'JEDI' at offset $0010 and $2010
    case 10003: // multicolor + loadaddr
      // $4000 => InterPaint multicolor
      // $6000 => Koala Painter
    case 10004:
      // $4000 => Face Paint (.fpt)
    case 10006:
      // $6000 => Run Paint (.rpm)
    case 10018:
      // $2000 => Advanced Art Studio
    case 10022:
      // $18DC => Micro Illustrator (uncompressed)
    case 10050:
      // $1800 => Picasso64
    case 10218:
      // $3C00 => Image System (.ism)
    case 10219:
      // $7800 => Saracen Paint (.sar)
      File_error = 0;
      break;
    case 10242:
      // $4000 => Artist 64 (.a64)
      // $A000 => Blazing paddles (.pi)
      // $5C00 => Rainbow Painter (.rp)
      if (load_addr != 0x4000 && load_addr != 0xa000 && load_addr != 0x5c00)
      {
        File_error = 1;
        return;
      }
      File_error = 0;
      break;
    case 10608:
      // $0801 = BASIC programs loading address
      File_error = 0;
      break;
    case 17218:
    case 17409:
      // $3c00 => FLI-designer v1.1
      // ? $3ff0 => FLI designer 2 ?
    case 17410:
     // $3c00 => FLI MATIC
    case 17474: // FLI (BlackMail) + loadaddr
      // $3b00 => FLI Graph 2
    case 17665:
      // $3b00 => FLI editor
    case 17666:
      // $3b00 => FLI Graph
    case 10277: // multicolor CDU-Paint + loadaddr
      // $7EEF
      File_error = 0;
      break;
    default: // then we don't know for now.
      if (load_addr == 0x6000 || load_addr == 0x5c00)
      {
        long unpacked_size;
        byte * buffer = GFX2_malloc(file_size);
        if (buffer == NULL)
          return;
        fseek(file, SEEK_SET, 0);
        if (!Read_bytes(file, buffer, file_size))
          return;
        unpacked_size = C64_unpack_doodle(&buffer, file_size);
        free(buffer);
        switch (unpacked_size)
        {
          case 9024:  // Doodle hi color
          case 9216:
          case 10001: // Koala painter 2
          case 10070:
            File_error = 0;
        }
      }
  }
}

/**
 * Test for a C64 auto-load machine language program
 * which could be a picture
 */
void Test_PRG(T_IO_Context * context, FILE * file)
{
  unsigned long file_size;
  word load_addr;
  (void)context;

  file_size = File_length_file(file);
  if (file_size > (38911 + 2))  // maximum length of PRG loaded at $0801
    return;
  if (!Read_word_le(file, &load_addr))
    return;
  if (load_addr != 0x0801)
    return;
  // 6502 emulators :
  // https://github.com/redcode/6502
  // http://rubbermallet.org/fake6502.c
  // https://github.com/jamestn/cpu6502
  // https://github.com/dennis-chen/6502-Emu
  // https://github.com/DavidBuchanan314/6502-emu
  // basic program
  if (C64_isBinaryProgram(file) != 0)
    File_error = 0;
}

/**
 * Load C64 hires (320x200)
 *
 * @param context	the IO context
 * @param bitmap the bitmap RAM (8000 bytes)
 * @param screen_ram the screen RAM (1000 bytes)
 */
static void Load_C64_hires(T_IO_Context *context, byte *bitmap, byte *screen_ram)
{
  int cx,cy,x,y,c[4],pixel,color;

  for(cy=0; cy<25; cy++)
  {
    for(cx=0; cx<40; cx++)
    {
      if(screen_ram != NULL)
      {
        c[0]=screen_ram[cy*40+cx]&15;
        c[1]=screen_ram[cy*40+cx]>>4;
      }
      else
      { /// If screen_ram is NULL, uses default C64 basic colors
        c[0] = 6;
        c[1] = 14;
      }
      for(y=0; y<8; y++)
      {
        pixel=bitmap[cy*320+cx*8+y];
        for(x=0; x<8; x++)
        {
          color=c[pixel&(1<<(7-x))?1:0];
          Set_pixel(context, cx*8+x,cy*8+y,color);
        }
      }
    }
  }
}

/**
 * Load C64 multicolor (160x200)
 *
 * @param context	the IO context
 * @param bitmap the bitmap RAM (8000 bytes)
 * @param screen_ram the screen RAM (1000 bytes)
 * @param color_ram the color RAM (1000 bytes)
 * @param background the background color
 */
static void Load_C64_multi(T_IO_Context *context, byte *bitmap, byte *screen_ram, byte *color_ram, byte background)
{
    int cx,cy,x,y,c[4],pixel,color;
    c[0]=background&15;
    for(cy=0; cy<25; cy++)
    {
        for(cx=0; cx<40; cx++)
        {
            c[1]=screen_ram[cy*40+cx]>>4;
            c[2]=screen_ram[cy*40+cx]&15;
            c[3]=color_ram[cy*40+cx]&15;

            for(y=0; y<8; y++)
            {
                pixel=bitmap[cy*320+cx*8+y];
                for(x=0; x<4; x++)
                {
                    color=c[(pixel&3)];
                    pixel>>=2;
                    Set_pixel(context, cx*4+(3-x),cy*8+y,color);
                }
            }
        }
    }
}

/**
 * Loads a C64 FLI (Flexible Line Interpretation) picture.
 * Sets 4 layers :
 *  - Layer 0 : filled with background colors (1 per line)
 *  - Layer 1 : "Color RAM" 4x8 blocks
 *  - Layer 2 : pixels (From Screen RAMs + Bitmap)
 *  - Layer 3 : Transparency layer filled with color 16
 *
 * @param context the IO context
 * @param bitmap 8000 bytes buffer
 * @param screen_ram 8 x 1024 bytes buffers
 * @param color_ram 1000 byte buffer
 * @param background 200 byte buffer
 */
void Load_C64_fli(T_IO_Context *context, byte *bitmap, byte *screen_ram, byte *color_ram, byte *background)
{
  // Thanks to MagerValp for complement of specifications.
  //
  // background : length: 200 (+ padding 56)
  //    These are the BG colors for lines 0-199 (top to bottom)
  //        Low nybble: the color.
  //        High nybble: garbage. ignore it.
  // color_ram  : length: 1000 (+ padding 24)
  //    Color RAM. Contains one color per 4x8 block.
  //    There are 40x25 such blocks, arranged from top left to bottom
  //    right, starting in right direction. For each block there is one byte.
  //        Low nybble: the color.
  //        High nybble: garbage. ignore it.
  // screen_ram : length: 8192
  //    Screen RAMs. The s is important.
  //    This is actually 8 blocks of 1000 bytes, each separated by a filler of
  //    24 bytes. Each byte contains data for a 4x1 pixel group, and a complete
  //    block will contain 40x25 of them. 40 is from left to right, and 25 is from
  //    top to bottom, spacing them 8 lines apart.
  //    The second block start at y=1, the third block starts at y=2, etc...
  //    Each byte contains 2 colors that *can* be used by the 4x1 pixel group:
  //        Low nybble: Color 1
  //        High nybble: Color 2
  //
  // bitmap     : length: 8000
  //    This is the final structure that refers to all others. It describes
  //    160x200 pixels linearly, from top left to bottom right, starting in
  //    right direction. For each pixel, two bits say which color is displayed
  //    (So 4 pixels are described by the same byte)
  //        00 Use the BG color of the current line (background[y])
  //        01 Use the Color 2 from the current 4x8 block of Screen RAM
  //           ((screen_ram[y/8][x/4] & 0xF0) >> 8)
  //        10 Use the Color 1 from the current 4x8 block of Screen RAM
  //           (screen_ram[y/8][x/4] & 0x0F)
  //        11 Use the color from Color RAM
  //           (color_ram[y/8][x/4] & 0x0F)
  //

  int cx,cy,x,y,c[4];

  if (context->Type == CONTEXT_MAIN_IMAGE)
  {
    // Fill layer 0 with background colors
    for(y=0; y<200; y++)
    {
      byte bg_color = 0;
      if (background != NULL)
        bg_color = background[y];
      for(x=0; x<160; x++)
        Set_pixel(context, x,y, bg_color);
    }

    // Fill layer 1 with color ram (1 color per 4x8 block)
    Set_loading_layer(context, 1);
    for(cy=0; cy<25; cy++)
    {
      for(cx=0; cx<40; cx++)
      {
        c[3]=color_ram[cy*40+cx]&15;
        for(y=0; y<8; y++)
        {
          for(x=0; x<4; x++)
          {
            Set_pixel(context, cx*4+x,cy*8+y,c[3]);
          }
        }
      }
    }
  }

  // Layer 2 are actual pixels
  Set_loading_layer(context, 2);
  for(cy=0; cy<25; cy++)
  {
    for(cx=0; cx<40; cx++)
    {
      c[3]=color_ram[cy*40+cx]&15;
      for(y=0; y<8; y++)
      {
        int pixel=bitmap[cy*320+cx*8+y];

        c[0] = 0;
        if(background != NULL)
          c[0] = background[cy*8+y]&15;
        c[1]=screen_ram[y*1024+cy*40+cx]>>4;
        c[2]=screen_ram[y*1024+cy*40+cx]&15;
        for(x=0; x<4; x++)
        {
          int color=c[(pixel&3)];
          pixel>>=2;
          Set_pixel(context, cx*4+(3-x),cy*8+y,color);
        }
      }
    }
  }
  if (context->Type == CONTEXT_MAIN_IMAGE)
  {
    // Fill layer 3 with color 16
    Set_loading_layer(context, 3);
    for(y=0; y<200; y++)
    {
      for(x=0; x<160; x++)
        Set_pixel(context, x,y,16);
    }
  }
}

/**
 * Count the length of the unpacked data
 *
 * RLE encoding is either ESCAPE CODE, COUNT, VALUE
 * or ESCAPE CODE, VALUE, COUNT
 *
 * @param buffer the packed data
 * @param input_size the packed data byte count
 * @param RLE_code the escape code
 * @param order 0 for ESCAPE, COUNT, VALUE, 1 for ESCAPE, VALUE, COUNT
 * @return the unpacked data byte count
 */
static long C64_unpack_get_length(const byte * buffer, long input_size, byte RLE_code, int order)
{
  const byte * end;
  long unpacked_size = 0;

  end = buffer + input_size;
  while(buffer < end)
  {
    if (*buffer == RLE_code)
    {
      if (order)
      { // ESCAPE, VALUE, COUNT
        buffer += 2;  // skip value
        unpacked_size += *buffer;
      }
      else
      { // ESCAPE, COUNT, VALUE
        buffer++;
        if (*buffer == 0)
          break;
        unpacked_size += *buffer++;
      }
    }
    else
      unpacked_size++;
    buffer++;
  }
  return unpacked_size;
}

/**
 * unpack RLE packed data
 *
 * RLE encoding is either ESCAPE CODE, COUNT, VALUE
 * or ESCAPE CODE, VALUE, COUNT
 *
 * @param unpacked buffer to received unpacked data
 * @param buffer the packed data
 * @param input_size the packed data byte count
 * @param RLE_code the escape code
 * @param order 0 for ESCAPE, COUNT, VALUE, 1 for ESCAPE, VALUE, COUNT
 */
static void C64_unpack(byte * unpacked, const byte * buffer, long input_size, byte RLE_code, int order)
{
  const byte * end;

  end = buffer + input_size;
  while(buffer < end)
  {
    if (*buffer == RLE_code)
    {
      byte count;
      byte value;
      buffer++;
      if (order)
      { // ESCAPE, VALUE, COUNT
        value = *buffer++;
        count = *buffer;
      }
      else
      { // ESCAPE, COUNT, VALUE
        count = *buffer++;
        value = *buffer;
      }
      if (count == 0)
        break;
      while (count-- > 0)
        *unpacked++ = value;
    }
    else
      *unpacked++ = *buffer;
    buffer++;
  }
}

/**
 * Unpack the Amica Paint RLE packing
 *
 * @param[in,out] file_buffer will contain the unpacked buffer on return
 * @param[in] file_size packed buffer size
 * @return the unpacked data size or -1 in case of error
 *
 * Ref:
 * - http://codebase64.org/doku.php?id=base:c64_grafix_files_specs_list_v0.03
 */
static long C64_unpack_amica(byte ** file_buffer, long file_size)
{
  long unpacked_size;
  byte * unpacked_buffer;
  const byte RLE_code = 0xC2;

  if (file_size <= 16 || file_buffer == NULL || *file_buffer == NULL)
    return -1;
  unpacked_size = C64_unpack_get_length(*file_buffer + 2, file_size - 2, RLE_code, 0);
  GFX2_Log(GFX2_DEBUG, "C64_unpack_amica() unpacked_size=%ld\n", unpacked_size);
   // 2nd pass to unpack
  unpacked_buffer = GFX2_malloc(unpacked_size);
  if (unpacked_buffer == NULL)
    return -1;
  C64_unpack(unpacked_buffer, *file_buffer + 2, file_size - 2, RLE_code, 0);

  free(*file_buffer);
  *file_buffer = unpacked_buffer;
  return unpacked_size;
}

/**
 * Unpack the DRAZPAINT RLE packing
 *
 * @param[in,out] file_buffer will contain the unpacked buffer on return
 * @param[in] file_size packed buffer size
 * @return the unpacked data size or -1 in case of error
 *
 * Ref:
 * - https://www.godot64.de/german/l_draz.htm
 * - https://sourceforge.net/p/view64/code/HEAD/tree/trunk/libview64.c#l2805
 */
static long C64_unpack_draz(byte ** file_buffer, long file_size)
{
  long unpacked_size;
  byte * unpacked_buffer;
  byte RLE_code;

  if (file_size <= 16 || file_buffer == NULL || *file_buffer == NULL)
    return -1;
  RLE_code = (*file_buffer)[15];
  // First pass to know unpacked size
  unpacked_size = C64_unpack_get_length(*file_buffer + 16, file_size - 16, RLE_code, 0);
  GFX2_Log(GFX2_DEBUG, "C64_unpack_draz() \"%.13s\" RLE code=$%02X RLE data length=%ld unpacked_size=%ld\n",
           *file_buffer + 2, RLE_code, file_size - 16, unpacked_size);
   // 2nd pass to unpack
  unpacked_buffer = GFX2_malloc(unpacked_size);
  if (unpacked_buffer == NULL)
    return -1;
  C64_unpack(unpacked_buffer, *file_buffer + 16, file_size - 16, RLE_code, 0);
  free(*file_buffer);
  *file_buffer = unpacked_buffer;
  return unpacked_size;
}

/**
 * Unpack doodle/koala painter 2 data
 *
 * @return the unpacked data size or -1 in case of error
 */
static long C64_unpack_doodle(byte ** file_buffer, long file_size)
{
  long unpacked_size;
  byte * unpacked_buffer;
  const byte RLE_code = 0xFE;

  if (file_size <= 16 || file_buffer == NULL || *file_buffer == NULL)
    return -1;
  // First pass to know unpacked size
  unpacked_size = C64_unpack_get_length(*file_buffer + 2, file_size - 2, RLE_code, 1);
  GFX2_Log(GFX2_DEBUG, "C64_unpack_doodle() unpacked_size=%ld\n", unpacked_size);
   // 2nd pass to unpack
  unpacked_buffer = GFX2_malloc(unpacked_size);
  if (unpacked_buffer == NULL)
    return -1;
  C64_unpack(unpacked_buffer, *file_buffer + 2, file_size - 2, RLE_code, 1);
  free(*file_buffer);
  *file_buffer = unpacked_buffer;
  return unpacked_size;
}

/**
 * Load C64 pictures formats.
 *
 * Supports:
 * - Hires (with or without ScreenRAM)
 * - Multicolor (Koala or CDU-paint format)
 * - FLI
 *
 * see http://unusedino.de/ec64/technical/formats/bitmap.html
 *
 * @param context the IO context
 */
void Load_C64(T_IO_Context * context)
{
    FILE* file;
    long file_size;
    byte hasLoadAddr=0;
    word load_addr;
    enum c64_format loadFormat = F_invalid;

    byte *file_buffer;
    byte *bitmap, *screen_ram, *color_ram=NULL, *background=NULL; // Only pointers to existing data
    byte *temp_buffer = NULL;
    word width, height=200;

    file = Open_file_read(context);

    if (file)
    {
        File_error=0;
        file_size = File_length_file(file);

        // Load entire file in memory
        file_buffer = GFX2_malloc(file_size);
        if (!file_buffer)
        {
            File_error = 1;
            fclose(file);
            return;
        }
        if (!Read_bytes(file,file_buffer,file_size))
        {
            File_error = 1;
            free(file_buffer);
            fclose(file);
            return;
        }
        fclose(file);

        // get load address (valid only if hasLoadAddr = 1)
        load_addr = file_buffer[0] | (file_buffer[1] << 8);

        // Unpack if needed
        if (memcmp(file_buffer + 2, "DRAZPAINT", 9) == 0)
          file_size = C64_unpack_draz(&file_buffer, file_size);
        else if(load_addr == 0x4000 && file_buffer[file_size-2] == 0xC2 && file_buffer[file_size-1] == 0)
          file_size = C64_unpack_amica(&file_buffer, file_size);
        else if (file_size < 8000 && (load_addr == 0x6000 || load_addr == 0x5c00))
          file_size = C64_unpack_doodle(&file_buffer, file_size);

        switch (file_size)
        {
            case 8000: // raw bitmap
                hasLoadAddr=0;
                loadFormat=F_bitmap;
                bitmap=file_buffer+0; // length: 8000
                screen_ram=NULL;
                break;

            case 8002: // raw bitmap with loadaddr
                hasLoadAddr=1;
                loadFormat=F_bitmap;
                bitmap=file_buffer+2; // length: 8000
                screen_ram=NULL;
                break;

            case 9000: // bitmap + ScreenRAM
                hasLoadAddr=0;
                loadFormat=F_hires;
                bitmap=file_buffer+0; // length: 8000
                screen_ram=file_buffer+8000; // length: 1000
                break;

            case 9003: // bitmap + ScreenRAM + loadaddr (+ border ?)
            case 9002: // bitmap + ScreenRAM + loadaddr
                hasLoadAddr=1;
                loadFormat=F_hires;
                bitmap=file_buffer+2; // length: 8000
                screen_ram=file_buffer+8002; // length: 1000
                break;

            case 9009: // Art Studio (.aas)
                hasLoadAddr=1;
                loadFormat=F_hires;
                bitmap=file_buffer+2; // length: 8000
                screen_ram=file_buffer+8002; // length: 1000
                break;

            case 9024:  // Doodle (unpacked from .jj)
            case 9216:
                hasLoadAddr=0;
                loadFormat=F_hires;
                screen_ram=file_buffer; // length: 1000 (+24 padding)
                bitmap=file_buffer+1024; // length: 8000
                break;

            case 9218: // Doodle (.dd)
                hasLoadAddr=1;
                loadFormat=F_hires;
                screen_ram=file_buffer+2; // length: 1000 (+24 padding)
                bitmap=file_buffer+1024+2; // length: 8000
                break;

            case 9332:  // Paint Magic .pmg
                hasLoadAddr=1;
                loadFormat=F_multi;
                // Display routine between offset $0002 and $0073 (114 bytes)
                // duplicated between offset      $2002 and $2073
                bitmap=file_buffer+114+2;         // $0074
                background=file_buffer+8000+114+2;// $1FB4
                temp_buffer = GFX2_malloc(1000);
                memset(temp_buffer, file_buffer[3+8000+114+2], 1000); // color RAM Byte
                color_ram=temp_buffer;
                //border  byte = file_buffer[4+8000+114+2];
                screen_ram=file_buffer+8192+114+2;  // $2074
                break;

            case 10001: // multicolor
            case 10070: // unpacked file.
                hasLoadAddr=0;
                loadFormat=F_multi;
                bitmap=file_buffer+0; // length: 8000
                screen_ram=file_buffer+8000; // length: 1000
                color_ram=file_buffer+9000; // length: 1000
                background=file_buffer+10000; // only 1
                break;

            case 10003: // multicolor + loadaddr
            case 10004: // extra byte is border color
            case 10006: // Run Paint
                hasLoadAddr=1;
                loadFormat=F_multi;
                bitmap=file_buffer+2; // length: 8000
                screen_ram=file_buffer+8002; // length: 1000
                color_ram=file_buffer+9002; // length: 1000
                background=file_buffer+10002; // only 1
                break;

            case 10018: // Advanced Art Studio (.ocp) + loadaddr
                hasLoadAddr=1;
                loadFormat=F_multi;
                bitmap=file_buffer+2; // length: 8000
                screen_ram=file_buffer+8000+2; // length: 1000
                color_ram=file_buffer+9016+2; // length: 1000
                // filebuffer+9000+2 is border
                background=file_buffer+9001+2; // only 1
                break;

            case 10022: // Micro Illustrator (.mil)
                hasLoadAddr=1;
                loadFormat=F_multi;
                screen_ram=file_buffer+20+2;
                color_ram=file_buffer+1000+20+2;
                bitmap=file_buffer+2*1000+20+2;
                break;

            case 10049: // unpacked DrazPaint
                hasLoadAddr=1;
                loadFormat=F_multi;
                color_ram=file_buffer; // length: 1000 + (padding 24)
                screen_ram=file_buffer+1024; // length: 1000 + (padding 24)
                bitmap=file_buffer+1024*2; // length: 8000
                background=file_buffer+8000+1024*2;
                break;

            case 10050: // Picasso64 multicolor + loadaddr
                hasLoadAddr=1;
                loadFormat=F_multi;
                color_ram=file_buffer+2; // length: 1000 + (padding 24)
                screen_ram=file_buffer+1024+2; // length: 1000 + (padding 24)
                bitmap=file_buffer+1024*2+2; // length: 8000
                background=file_buffer+1024*2+2-1; // only 1
                break;

            case 10218: // Image System
                hasLoadAddr=1;
                loadFormat=F_multi;
                color_ram=file_buffer+2; // Length: 1000 (+ padding 24)
                bitmap=file_buffer+1024+2; // Length: 8000 (+padding 192)
                screen_ram=file_buffer+8192+1024+2;  // Length: 1000 (no padding)
                background=file_buffer+8192+1024+2-1; // only 1
                break;

            case 10219: // Saracen Paint (.sar)
                hasLoadAddr=1;
                loadFormat=F_multi;
                screen_ram=file_buffer+2;  // Length: 1000 (+ padding24)
                background=file_buffer+1008+2; // offset 0x3F0 (only 1 byte)
                bitmap=file_buffer+1024+2; // Length: 8000 (+padding 192)
                color_ram=file_buffer+8192+1024+2; // Length: 1000 (+ padding 24)
                break;

            case 10242: // Artist 64/Blazing Paddles/Rainbow Painter multicolor + loadaddr
                hasLoadAddr=1;
                loadFormat=F_multi;
                switch(load_addr)
                {
                  default:
                  case 0x4000:  // Artist 64
                    bitmap=file_buffer+2; // length: 8000 (+padding 192)
                    screen_ram=file_buffer+8192+2; // length: 1000 + (padding 24)
                    color_ram=file_buffer+1024+8192+2; // length: 1000 + (padding 24)
                    background=file_buffer+1024*2+8192+2-1; // only 1
                    break;
                  case 0xa000:  // Blazing Paddles
                    bitmap=file_buffer+2; // length: 8000 (+padding 192)
                    screen_ram=file_buffer+8192+2; // length: 1000 + (padding 24)
                    color_ram=file_buffer+1024+8192+2; // length: 1000 + (padding 24)
                    background=file_buffer+8064+2; // only 1
                    break;
                  case 0x5c00:  // Rainbow Painter
                    screen_ram=file_buffer+2; // length: 1000 + (padding 24)
                    bitmap=file_buffer+1024+2; // length: 8000 (+padding 192)
                    color_ram=file_buffer+1024+8192+2; // length: 1000 + (padding 24)
                    background=file_buffer; // only 1
                    break;
                }
                break;

            case 10257: // unpacked Amica Paint (.ami)
                hasLoadAddr=1;
                loadFormat=F_multi;
                bitmap=file_buffer; // length 8000
                screen_ram=file_buffer+8000;  // length: 1000
                color_ram=file_buffer+1000+8000;// length:1000
                background=file_buffer+2*1000+8000;//1
                // remaining bytes (offset 10001, length 256) are a "Color Rotation Table"
                // we should decode if we learn its format...
                break;

            case 10277: // multicolor CDU-Paint + loadaddr
                hasLoadAddr=1;
                loadFormat=F_multi;
                // 273 bytes of display routine
                bitmap=file_buffer+275; // length: 8000
                screen_ram=file_buffer+8275; // length: 1000
                color_ram=file_buffer+9275; // length: 1000
                background=file_buffer+10275; // only 1
                break;

            case 10608: // prg
                hasLoadAddr=1;
                loadFormat=F_multi;
                bitmap = file_buffer + 0x239;
                // border = bitmap + 8000
                background = bitmap + 8000 + 1;
                screen_ram = bitmap + 8000 + 2;
                color_ram = screen_ram + 1000;
                break;

            case 17472: // FLI (BlackMail)
                hasLoadAddr=0;
                loadFormat=F_fli;
                background=file_buffer+0; // length: 200 (+ padding 56)
                color_ram=file_buffer+256; // length: 1000 (+ padding 24)
                screen_ram=file_buffer+1280; // length: 8192
                bitmap=file_buffer+9472; // length: 8000
                break;

            case 17474: // FLI (BlackMail) + loadaddr
                hasLoadAddr=1;
                loadFormat=F_fli;
                background=file_buffer+2; // length: 200 (+ padding 56)
                color_ram=file_buffer+258; // length: 1000 (+ padding 24)
                screen_ram=file_buffer+1282; // length: 8192
                bitmap=file_buffer+9474; // length: 8000
                break;

            case 17218:
            case 17409: // FLI-Designer v1.1 (+loadaddr)
            case 17410: // => FLI MATIC (background at 2+1024+8192+8000+65 ?)
              hasLoadAddr=1;
              loadFormat=F_fli;
              background=NULL;
              color_ram=file_buffer+2; // length: 1000 (+ padding 24)
              screen_ram=file_buffer+1024+2; // length: 8192
              bitmap=file_buffer+8192+1024+2; // length: 8000
              break;

            case 17666: // FLI Graph
              hasLoadAddr=1;
              loadFormat=F_fli;
              background=file_buffer+2;
              color_ram=file_buffer+256+2; // length: 1000 (+ padding 24)
              screen_ram=file_buffer+1024+256+2; // length: 8192
              bitmap=file_buffer+8192+1024+256+2; // length: 8000
              break;

            case 17665: // FLI Editor
              hasLoadAddr=1;
              loadFormat=F_fli;
              background=file_buffer+8;
              color_ram=file_buffer+256+2; // length: 1000 (+ padding 24)
              screen_ram=file_buffer+1024+256+2; // length: 8192
              bitmap=file_buffer+8192+1024+256+2; // length: 8000
              break;

            default:
                File_error = 1;
                free(file_buffer);
                return;
        }

        if (loadFormat == F_invalid)
        {
          File_error = 1;
          free(file_buffer);
          return;
        }

        if (loadFormat == F_fli || loadFormat == F_multi)
        {
          context->Ratio = PIXEL_WIDE;
          width = 160;
        }
        else
        {
          context->Ratio = PIXEL_SIMPLE;
          width = 320;
        }

        // Write detailed format in comment
        if (hasLoadAddr)
          snprintf(context->Comment,COMMENT_SIZE+1,"%s, load at $%4.4X",c64_format_names[loadFormat],load_addr);
        else
          snprintf(context->Comment,COMMENT_SIZE+1,"%s, no addr",c64_format_names[loadFormat]);

        Pre_load(context, width, height, file_size, FORMAT_C64, context->Ratio, (loadFormat == F_bitmap) ? 1 : 4); // Do this as soon as you can

        if (Config.Clear_palette)
          memset(context->Palette,0, sizeof(T_Palette));
        C64_set_palette(context->Palette);
        context->Transparent_color=16;

        switch(loadFormat)
        {
          case F_fli:
            Load_C64_fli(context,bitmap,screen_ram,color_ram,background);
            Set_image_mode(context, IMAGE_MODE_C64FLI);
            break;
          case F_multi:
            Load_C64_multi(context,bitmap,screen_ram,color_ram,
                           (background==NULL) ? 0 : *background);
            Set_image_mode(context, IMAGE_MODE_C64MULTI);
            break;
          default:
            Load_C64_hires(context,bitmap,screen_ram);
            if (loadFormat == F_hires)
              Set_image_mode(context, IMAGE_MODE_C64HIRES);
        }

        free(file_buffer);
        if (temp_buffer)
          free(temp_buffer);
    }
    else
        File_error = 1;
}

/**
 * Load C64 autoload pictures
 *
 * @param context the IO context
 */
void Load_PRG(T_IO_Context * context)
{
  FILE* file;
  unsigned long file_size;
  struct c64state c64;
  enum c64_format loadFormat = F_invalid;
  word load_addr;
  word width, height = 200;

  memset(&c64, 0, sizeof(c64));

  File_error = 1;
  file = Open_file_read(context);
  if (file == NULL)
    return;
  file_size = File_length_file(file);
  if (!Read_word_le(file, &load_addr))
    return;
  if (load_addr == 0x801)
  {
    word start_addr = C64_isBinaryProgram(file);
    if (start_addr == 0)
      return;
    if (fseek(file, 2, SEEK_SET) < 0)
      return;
    if (C64_LoadPrg(&c64, file, start_addr))
    {
      File_error = 0;
      if (c64.vicmode & C64_VICMODE_FLI)
        loadFormat = F_fli;
      else if (c64.vicmode & C64_VICMODE_MULTI)
        loadFormat = F_multi;
      else
        loadFormat = F_hires;

      if (loadFormat == F_fli || loadFormat == F_multi)
      {
        context->Ratio = PIXEL_WIDE;
        width = 160;
      }
      else
      {
        context->Ratio = PIXEL_SIMPLE;
        width = 320;
      }

      Pre_load(context, width, height, file_size, FORMAT_PRG, context->Ratio, 4); // Do this as soon as you can

      if (Config.Clear_palette)
        memset(context->Palette, 0, sizeof(T_Palette));
      C64_set_palette(context->Palette);
      context->Transparent_color = 16;

      switch(loadFormat)
      {
        case F_fli:
          Load_C64_fli(context, c64.ram + c64.bitmap, c64.ram + c64.screen, c64.ram + 0xd800, c64.backgrounds);
          Set_image_mode(context, IMAGE_MODE_C64FLI);
          break;
        case F_multi:
          Load_C64_multi(context, c64.ram + c64.bitmap, c64.ram + c64.screen, c64.ram + 0xd800, c64.ram[0xd021]);
          Set_image_mode(context, IMAGE_MODE_C64MULTI);
          break;
        default:
          Load_C64_hires(context, c64.ram + c64.bitmap, c64.ram + c64.screen);
          if (loadFormat == F_hires)
            Set_image_mode(context, IMAGE_MODE_C64HIRES);
      }
    }
    if (c64.ram != NULL)
      free(c64.ram);
  }
}

/**
 * Display the dialog for C64 save parameters
 *
 * @param[in,out] saveFormat one of the C64 mode from @ref c64_format
 * @param[in,out] saveWhat 0=All, 1=Only bitmap, 2=Only Screen RAM, 3=Only color RAM
 * @param[in,out] loadAddr actual load address or 0 for "None"
 * @return true to proceed, false to abort
 */
static int Save_C64_window(enum c64_format *saveFormat, byte *saveWhat, word *loadAddr)
{
  int button;
  unsigned int i;
  T_Dropdown_button *what, *addr;
  T_Dropdown_button *format;
  static const char * what_label[] = {
    "All",
    "Bitmap",
    "Screen",
    "Color"
  };
  static const char * address_label[] = {
    "None",
    "$2000",
    "$4000",
    "$6000",
    "$8000",
    "$A000",
    "$C000",
    "$E000"
  };
  // default addresses :
  //  - FLI Fli Graph 2 (BlackMail) => $3b00
  //  - multicolor (Koala Painter) => $6000
  //  - hires (InterPaint) => $4000

  Open_window(200,120,"C64 saving settings");
  Window_set_normal_button(110,100,80,15,"Save",1,1,KEY_RETURN); // 1
  Window_set_normal_button(10,100,80,15,"Cancel",1,1,KEY_ESCAPE); // 2

  Print_in_window(13,18,"Data:",MC_Dark,MC_Light);
  what = Window_set_dropdown_button(10,28,90,15,70,what_label[*saveWhat],1, 0, 1, LEFT_SIDE,0); // 3
  Window_dropdown_clear_items(what);
  for (i=0; i<sizeof(what_label)/sizeof(what_label[0]); i++)
    Window_dropdown_add_item(what,i,what_label[i]);

  Print_in_window(113,18,"Address:",MC_Dark,MC_Light);
  addr = Window_set_dropdown_button(110,28,70,15,70,address_label[*loadAddr/0x2000],1, 0, 1, LEFT_SIDE,0); // 4
  Window_dropdown_clear_items(addr);
  for (i=0; i<sizeof(address_label)/sizeof(address_label[0]); i++)
    Window_dropdown_add_item(addr,i,address_label[i]);

  Print_in_window(13,46,"Format:",MC_Dark,MC_Light);
  format = Window_set_dropdown_button(10,56,90,15,88,c64_format_names[*saveFormat],1, 0, 1, LEFT_SIDE,0); // 5
  if (*saveFormat == F_hires || *saveFormat == F_bitmap)
  {
    Window_dropdown_add_item(format, F_hires, c64_format_names[F_hires]);
    Window_dropdown_add_item(format, F_bitmap, c64_format_names[F_bitmap]);
  }
  else
  {
    Window_dropdown_add_item(format, F_multi, c64_format_names[F_multi]);
    Window_dropdown_add_item(format, F_fli, c64_format_names[F_fli]);
  }

  Update_window_area(0,0,Window_width,Window_height);
  Display_cursor();

  do
  {
    button = Window_clicked_button();
    if (Is_shortcut(Key, 0x100+BUTTON_HELP))
    {
      Key = 0;
      Window_help(BUTTON_SAVE, "COMMODORE 64 FORMATS");
    }
    else switch(button)
    {
      case 3: // Save what
        *saveWhat = Window_attribute2;
        GFX2_Log(GFX2_DEBUG, "Save_C64_Window() : what=%d (%s)\n", *saveWhat, what_label[*saveWhat]);
        break;

      case 4: // Load addr
        *loadAddr = Window_attribute2*0x2000;
        GFX2_Log(GFX2_DEBUG, "Save_C64_Window() : addr=$%04x (%d)\n",*loadAddr,Window_attribute2);
        break;

      case 5:
        *saveFormat = Window_attribute2;
        GFX2_Log(GFX2_DEBUG, "Save_C64_Window() : format=%d\n", Window_attribute2);
        break;

      case 0: break;
    }
  } while(button!=1 && button!=2);

  Close_window();
  Display_cursor();
  return button==1;
}

/// Save a C64 hires picture
///
/// c64 hires is 320x200 with only 2 colors per 8x8 block.
static int Save_C64_hires(T_IO_Context *context, byte saveWhat, word loadAddr)
{
  int i, pos = 0;
  word cx, cy, x, y;
  byte screen_ram[1000],bitmap[8000];
  FILE *file;

  for(cy=0; cy<25; cy++) // Character line, 25 lines
  {
    for(cx=0; cx<40; cx++) // Character column, 40 columns
    {
      byte fg, bg;  // foreground and background colors for the 8x8 block
      byte c[2];
      int count = 0;
      // first pass : find colors used
      for(y=0; y<8; y++)
      {
        for(x=0; x<8; x++)
        {
          byte pixel = Get_pixel(context, x+cx*8,y+cy*8);
          if(pixel>15)
          {
            Warning_message("Color above 15 used");
            // TODO hilite offending block here too?
            // or make it smarter with color allocation?
            // However, the palette is fixed to the 16 first colors
            return 1;
          }
          for (i = 0; i < count; i++)
          {
            if (c[i] == pixel)
              break;
          }
          if (i >= 2)
          {
            Warning_with_format("More than 2 colors\nin 8x8 pixel cell: (%d, %d)\nRect: (%d, %d, %d, %d)", cx, cy, cx * 8, cy * 8, cx * 8 + 7, cy * 8 + 7);
            // TODO here we should hilite the offending block
            return 1;
          }
          if (i >= count)
            c[count++] = pixel;
        }
      }

      if (count == 1)
      {
        if (c[0] == 0)  // only black
          fg = 1; // white
        else
          fg = c[0];
        bg = 0; // black
      }
      else
      {
        // set lower color index as background
        if (c[0] < c[1])
        {
          fg = c[1];
          bg = c[0];
        }
        else
        {
          fg = c[0];
          bg = c[1];
        }
      }
      screen_ram[cx+cy*40] = (fg<<4) | bg;

      // 2nd pass : store bitmap (0 = background, 1 = foreground)
      for(y=0; y<8; y++)
      {
        byte bits = 0;
        for(x=0; x<8; x++)
        {
          bits <<= 1;
          if (Get_pixel(context, x+cx*8, y+cy*8) == fg)
            bits |= 1;
        }
        bitmap[pos++] = bits;
      }
    }
  }

  file = Open_file_write(context);

  if(!file)
  {
    Warning_message("File open failed");
    File_error = 1;
    return 1;
  }

  if (loadAddr)
    Write_word_le(file,loadAddr);

  if (saveWhat==0 || saveWhat==1)
    Write_bytes(file,bitmap,8000);
  if (saveWhat==0 || saveWhat==2)
    Write_bytes(file,screen_ram,1000);

  fclose(file);
  return 0;
}


/**
 * Save a C64 FLI (Flexible Line Interpretation) picture.
 *
 * This function is able to save a one layer picture, by finding
 * itself the background colors and color RAM value to be used.
 *
 * The algorithm is :
 * - first choose the lowest value for all possible background colors for each line
 * - first the lowest value from the possible colors for color RAM
 * - encode bitmap and screen RAMs
 *
 * The algorithm can fail by picking a "wrong" background color for a line,
 * that make the choice for the color RAM value of one of the 40 blocks impossible.
 *
 * @param context the IO context
 * @param saveWhat what part of the data to save
 * @param loadAddr The load address
 */
int Save_C64_fli_monolayer(T_IO_Context *context, byte saveWhat, word loadAddr)
{
  FILE * file;
  byte bitmap[8000],screen_ram[1024*8],color_ram[1024];
  byte background[256];

  memset(bitmap, 0, sizeof(bitmap));
  memset(screen_ram, 0, sizeof(screen_ram));
  memset(color_ram, 0, sizeof(color_ram));
  memset(background, 0, sizeof(background));

  memset(color_ram, 0xff, 40*25); // no hint
  memset(background, 0xff, 200);

  if (C64_pixels_to_FLI(bitmap, screen_ram, color_ram, background, context->Target_address, context->Pitch, 0) > 0)
    return 1;

  file = Open_file_write(context);

  if(!file)
  {
    Warning_message("File open failed");
    File_error = 1;
    return 1;
  }

  if (loadAddr)
    Write_word_le(file, loadAddr);

  if (saveWhat==0)
    Write_bytes(file,background,256);    // Background colors for lines 0-199 (+ 56bytes padding)

  if (saveWhat==0 || saveWhat==3)
    Write_bytes(file,color_ram,1024); // Color RAM (1000 bytes + padding 24)

  if (saveWhat==0 || saveWhat==1)
    Write_bytes(file,screen_ram,8192);  // Screen RAMs 8 x (1000 bytes + padding 24)

  if (saveWhat==0 || saveWhat==2)
    Write_bytes(file,bitmap,8000);  // BitMap

  fclose(file);

  return 0;
}

/**
 * Save a C64 multicolor picture
 *
 * @param context the IO context
 * @param saveWhat what part of the data to save
 * @param loadAddr The load address
 */
int Save_C64_multi(T_IO_Context *context, byte saveWhat, word loadAddr)
{
    /*
    BITS     COLOR INFORMATION COMES FROM
    00     Background color #0 (screen color)
    01     Upper 4 bits of Screen RAM
    10     Lower 4 bits of Screen RAM
    11     Color RAM nybble (nybble = 1/2 byte = 4 bits)
    */

    int cx,cy,x,y,c[4]={0,0,0,0},color,lut[16],bits,pixel,pos=0;
    int cand,n,used;
    word cols, candidates = 0, invalids = 0;

    // FIXME allocating this on the stack is not a good idea. On some platforms
    // the stack has a rather small size...
    byte bitmap[8000],screen_ram[1000],color_ram[1000];

    word numcolors;
    dword cusage[256];
    byte i,background=0;
    FILE *file;

    // Detect the background color the image should be using. It's the one that's
    // used on all tiles having 4 colors.
    for(y=0;y<200;y=y+8)
    {
        for (x = 0; x<160; x=x+4)
        {
            cols = 0;

            // Compute the usage count of each color in the tile
            for (cy=0;cy<8;cy++)
            for (cx=0;cx<4;cx++)
            {
              pixel=Get_pixel(context, x+cx,y+cy);
              if(pixel>15)
              {
                Warning_message("Color above 15 used");
                // TODO hilite as in hires, you should stay to
                // the fixed 16 color palette
                return 1;
              }
              cols |= (1 << pixel);
            }

            cand = 0;
            used = 0;
            // Count the number of used colors in the tile
            for (n = 0; n<16; n++)
            {
                if (cols & (1 << n))
                    used++;
            }

            if (used>3)
            {
                GFX2_Log(GFX2_DEBUG, "(%3d,%3d) used=%d cols=%04x\n", x, y, used,(unsigned)cols);
                // This is a tile that uses the background color (and 3 others)

                // Try to guess which color is most likely the background one
                for (n = 0; n<16; n++)
                {
                    if ((cols & (1 << n)) && !((candidates | invalids) & (1 << n))) {
                        // This color is used in this tile but
                        // was not used in any other tile yet,
                        // so it could be the background one.
                        candidates |= 1 << n;
                    }

                    if ((cols & (1 << n)) == 0 ) {
                        // This color isn't used at all in this tile:
                        // Can't be the global background
                        invalids |= 1 << n;
                        candidates &= ~(1 << n);
                    }

                    if (candidates & (1 << n)) {
                        // We have a candidate, mark it as such
                        cand++;
                    }
                }

                // After checking the constraints for this tile, do we have
                // candidate background colors left ?
                if (cand==0)
                {
                    Warning_message("No possible global background color");
                    return 1;
                }
            }
        }
    }

	// Now just pick the first valid candidate
	for (n = 0; n<16; n++)
	{
		if (candidates & (1 << n)) {
			background = n;
			break;
		}
	}
  GFX2_Log(GFX2_DEBUG, "Save_C64_multi() background=%d ($%x) candidates=%x invalid=%x\n",
           (int)background, (int)background, (unsigned)candidates, (unsigned)invalids);


  // Now that we know which color is the background, we can encode the cells
  for(cy=0; cy<25; cy++)
  {
    for(cx=0; cx<40; cx++)
    {
      numcolors=Count_used_colors_area(cusage,cx*4,cy*8,4,8);
      if(numcolors>4)
      {
        Warning_with_format("More than 4 colors\nin 4x8 pixel cell: (%d, %d)\nRect: (%d, %d, %d, %d)", cx, cy, cx * 4, cy * 8, cx * 4 + 3, cy * 8 + 7);
        // TODO hilite offending block
        return 1;
      }
      color=1;
      c[0]=background;
      for(i=0; i<16; i++)
      {
        lut[i]=0;
        if(cusage[i] && (i!=background))
        {
          lut[i]=color;
          c[color]=i;
          color++;
        }
      }
      // add to screen_ram and color_ram
      screen_ram[cx+cy*40]=c[1]<<4|c[2];
      color_ram[cx+cy*40]=c[3];

      for(y=0;y<8;y++)
      {
        bits=0;
        for(x=0;x<4;x++)
        {
          pixel = Get_pixel(context, cx*4+x,cy*8+y);
          bits = (bits << 2) | lut[pixel];
        }
        bitmap[pos++]=bits;
      }
    }
  }

  file = Open_file_write(context);

  if(!file)
  {
    Warning_message("File open failed");
    File_error = 2;
    return 2;
  }

  setvbuf(file, NULL, _IOFBF, 64*1024);

  if (loadAddr)
    Write_word_le(file,loadAddr);

  if (saveWhat==0 || saveWhat==1)
    Write_bytes(file,bitmap,8000);

  if (saveWhat==0 || saveWhat==2)
    Write_bytes(file,screen_ram,1000);

  if (saveWhat==0 || saveWhat==3)
    Write_bytes(file,color_ram,1000);

  if (saveWhat==0)
    Write_byte(file,background);

  fclose(file);
  return 0;
}

/**
 * Save a C64 FLI (Flexible Line Interpretation) picture.
 *
 * This function need a 3 layer image :
 * - layer 0 is background colors
 * - layer 1 is color RAM values (4x8 blocks)
 * - layer 2 is the actual picture
 *
 * @param context the IO context
 * @param saveWhat what part of the data to save
 * @param loadAddr The load address
 */
int Save_C64_fli(T_IO_Context * context, byte saveWhat, word loadAddr)
{
  FILE *file;
  byte file_buffer[17474];

  memset(file_buffer,0,sizeof(file_buffer));

  switch(C64_FLI(context, file_buffer+9474, file_buffer+1282, file_buffer+258, file_buffer+2))
  {
    case 0: // OK
      break;
    case 1:
      Warning_message("Less than 3 layers");
      File_error=1;
      return 1;
    case 2:
      Warning_message("Picture must be 160x200");
      File_error=1;
      return 1;
    default:
      File_error=1;
      return 1;
  }

  file = Open_file_write(context);

  if(!file)
  {
    Warning_message("File open failed");
    File_error = 1;
    return 1;
  }

  if (loadAddr)
    Write_word_le(file, loadAddr);

  if (saveWhat==0)
    Write_bytes(file,file_buffer+2,256);    // Background colors for lines 0-199 (+ 56bytes padding)

  if (saveWhat==0 || saveWhat==3)
    Write_bytes(file,file_buffer+258,1024); // Color RAM (1000 bytes + padding 24)

  if (saveWhat==0 || saveWhat==1)
    Write_bytes(file,file_buffer+1282,8192);  // Screen RAMs 8 x (1000 bytes + padding 24)

  if (saveWhat==0 || saveWhat==2)
    Write_bytes(file,file_buffer+9474,8000);  // BitMap

  fclose(file);
  return 0;
}

/**
 * Save C64 picture.
 *
 * Supports :
 * - HiRes (320x200)
 * - Multicolor
 * - FLI
 *
 * @param context the IO context
 */
void Save_C64(T_IO_Context * context)
{
  enum c64_format saveFormat = F_invalid;
  static byte saveWhat=0;
  static word loadAddr=0;

  if (((context->Width!=320) && (context->Width!=160)) || context->Height!=200)
  {
    Warning_message("must be 320x200 or 160x200");
    File_error = 1;
    return;
  }

  saveFormat = (context->Width == 320) ? F_hires : F_multi;

  GFX2_Log(GFX2_DEBUG, "Save_C64() extension : %s\n", context->File_name + strlen(context->File_name) - 4);
  if (strcasecmp(context->File_name + strlen(context->File_name) - 4, ".fli") == 0)
    saveFormat = F_fli;

  if(!Save_C64_window(&saveFormat, &saveWhat,&loadAddr))
  {
    File_error = 1;
    return;
  }

  Set_saving_layer(context, 0);
  switch (saveFormat)
  {
    case F_fli:
      if (context->Nb_layers < 3)
        File_error = Save_C64_fli_monolayer(context, saveWhat, loadAddr);
      else
        File_error = Save_C64_fli(context, saveWhat, loadAddr);
      break;
    case F_multi:
      File_error = Save_C64_multi(context, saveWhat, loadAddr);
      break;
    case F_bitmap:
      saveWhat = 1; // force save bitmap
#if defined(__GNUC__) && (__GNUC__ >= 7)
      __attribute__ ((fallthrough));
#endif
    case F_hires:
    default:
      File_error = Save_C64_hires(context, saveWhat, loadAddr);
  }
}


/////////////////////////// pixcen *.GPX ///////////////////////////
void Test_GPX(T_IO_Context * context, FILE * file)
{
  byte header[2];
  (void)context;

  // check for a Zlib compressed stream
  File_error = 1;
  if (!Read_bytes(file, header, 2))
    return;
  if ((header[0] & 0x0f) != 8)
    return;
  if (((header[0] << 8) + header[1]) % 31)
    return;
  File_error = 0;
}

void Load_GPX(T_IO_Context * context)
{
  FILE * file;
  unsigned long file_size;
  byte * buffer;

  File_error = 1;
  file = Open_file_read(context);
  if (file == NULL)
    return;
  file_size = File_length_file(file);
  buffer = GFX2_malloc(file_size);
  if (buffer == NULL)
  {
    fclose(file);
    return;
  }
  if (Read_bytes(file, buffer, file_size))
  {
    byte * gpx = NULL;
    unsigned long gpx_size = 0;
    int r = Z_MEM_ERROR;

    do
    {
      free(gpx);
      gpx_size += 65536;
      gpx = GFX2_malloc(gpx_size);
      if (gpx == NULL)
        break;
      r = uncompress(gpx, &gpx_size, buffer, file_size);
      if (r != Z_BUF_ERROR && r != Z_OK)
        GFX2_Log(GFX2_ERROR, "uncompress() failed with error %d: %s\n", r, zError(r));
    }
    while (r == Z_BUF_ERROR); // there was not enough room in the output buffer
    if (r == Z_OK)
    {
      byte * p;
      dword version, mode;
/*
 mode :
0		BITMAP,
1		MC_BITMAP,
2		SPRITE,
3		MC_SPRITE,
4		CHAR,
5   MC_CHAR,
6		UNUSED1,
7		UNUSED2,
8		UNRESTRICTED,
9		W_UNRESTRICTED
*/
      GFX2_Log(GFX2_DEBUG, "inflated %lu bytes to %lu\n", file_size, gpx_size);
#define READU32LE(p) ((p)[0] | (p)[1] << 8 | (p)[2] << 16 | (p)[3] << 24)
      version = READU32LE(gpx);
      mode = READU32LE(gpx+4);
      GFX2_Log(GFX2_DEBUG, "gpx version %u mode %u\n", version, mode);
      snprintf(context->Comment, COMMENT_SIZE, "pixcen file version %u mode %u", version, mode);
      if (version >= 4)
      {
        dword count;
        const char * key;
        word value[256];
        int xsize = -1;
        int ysize = -1;
        int mapsize = -1;
        int screensize = -1;
        int colorsize = -1;
        int backbuffers = -1;

        count = READU32LE(gpx+8);
        p = gpx + 12;
        while (count--)
        {
          int i = 0;
          int int_value = 0;

          key = (const char *)p;
          while (*p++);
          for (;;)
          {
            value[i] = p[0] + (p[1] << 8);
            p += 2;
            if (value[i] == 0)
              break;
            int_value = int_value * 10 + (value[i] - '0');
            i++;
          }
          GFX2_Log(GFX2_DEBUG, "%s=%d\n", key, int_value);
          if (0 == strcmp(key, "xsize"))
            xsize = int_value;
          else if (0 == strcmp(key, "ysize"))
            ysize = int_value;
          else if (0 == strcmp(key, "mapsize"))
            mapsize = int_value;
          else if (0 == strcmp(key, "screensize"))
            screensize = int_value;
          else if (0 == strcmp(key, "colorsize"))
            colorsize = int_value;
          else if (0 == strcmp(key, "backbuffers"))
            backbuffers = int_value;
        }
//buffersize = 64 + (64 + mapsize + screensize + colorsize) * backbuffers;
        p += 64;  // 64 empty bytes ?
        File_error = 0;
        if (mode & 1)
          context->Ratio = PIXEL_WIDE;
        else
          context->Ratio = PIXEL_SIMPLE;
        Pre_load(context, xsize, ysize, file_size, FORMAT_GPX, context->Ratio, 4); // Do this as soon as you can
        if (Config.Clear_palette)
          memset(context->Palette,0, sizeof(T_Palette));
        C64_set_palette(context->Palette);
        context->Transparent_color=16;

        //foreach backbuffer
        if (backbuffers >= 1)
        {
          byte border, background;
          //byte ext0, ext1, ext2;
          byte * bitmap, * color, * screen;

          //GFX2_LogHexDump(GFX2_DEBUG, "GPX ", p, 0, 64);
          p += 47;  // Extra bytes
          //crippled = p;
          p += 6;
          //lock = p;
          p += 6;
          border = *p++;
          background = *p++;
          /*ext0 = *p++;
          ext1 = *p++;
          ext2 = *p++;*/
          p += 3;
          bitmap = p;
          p += mapsize;
          color = p;
          p += colorsize;
          screen = p;
          p += screensize;

          GFX2_Log(GFX2_DEBUG, "background color #%d, border color #%d\n", (int)background, (int)border);
          Load_C64_multi(context, bitmap, screen, color, background);
          Set_image_mode(context, (mode & 1) ? IMAGE_MODE_C64MULTI : IMAGE_MODE_C64HIRES);
        }
      }
      else
      {
        GFX2_Log(GFX2_ERROR, "GPX file version %d unsupported\n", version);
      }
    }
    free(gpx);
  }
  free(buffer);
  fclose(file);
}

/**
 * Test for SCR file (Amstrad CPC)
 *
 * SCR file format is from "Advanced OCP Art Studio" :
 * http://www.cpcwiki.eu/index.php/Format:Advanced_OCP_Art_Studio_File_Formats
 *
 * .WIN "window" format is also supported.
 *
 * For now we check the presence of a valid PAL file.
 * If the PAL file is not there the pixel data may still be valid.
 * The file size depends on the screen resolution.
 * An AMSDOS header would be a good indication but in some cases it may not
 * be there.
 */
void Test_SCR(T_IO_Context * context, FILE * file)
{
  // http://orgams.wikidot.com/le-format-impdraw-v2
  // http://orgams.wikidot.com/les-fichiers-win-compatibles-ocp-art-studio
  FILE * pal_file;
  unsigned long pal_size, file_size;
  byte mode, color_anim_flag;
  word loading_address = 0;

  File_error = 1;

  if (CPC_check_AMSDOS(file, &loading_address, &file_size))
  {
    if (loading_address == 0x170) // iMPdraw v2
    {
      byte buffer[0x90];
      fseek(file, 128, SEEK_SET); // right after AMSDOS header
      Read_bytes(file, buffer, 0x90);
      GFX2_LogHexDump(GFX2_DEBUG, "", buffer, 0, 0x90);
      File_error = 0;
      return;
    }
    else if ((loading_address == 0x200 || loading_address == 0xc000) && file_size > 16000)
    {
      File_error = 0;
      return;
    }
  }
  else
    file_size = File_length_file(file);

  if (file_size > 16384*2)
    return;

  // requires the PAL file
  pal_file = Open_file_read_with_alternate_ext(context, "pal");
  if (pal_file == NULL)
    return;
  /** @todo the palette data can be hidden in the 48 "empty" bytes
   * every 2048 bytes of a standard resolution SCR file.
   * So we should detect the hidden Z80 code and load them.
   * Load address of file is C000. Z80 code :<br>
   * <tt>C7D0: 3a d0 d7 cd 1c bd 21 d1 d7 46 48 cd 38 bc af 21 | :.....!..FH.8..!</tt><br>
   * <tt>C7E0: d1 d7 46 48 f5 e5 cd 32 bc e1 f1 23 3c fe 10 20 | ..FH...2...#<.. </tt><br>
   * <tt>C7F0: f1 c3 18 bb 00 00 00 00 00 00 00 00 00 00 00 00 | ................</tt><br>
   * mode and palette :<br>
   * <tt>D7D0: 00 1a 00 0c 03 0b 01 0d 17 10 02 0f 09 19 06 00 | ................</tt><br>
   * https://gitlab.com/GrafX2/grafX2/merge_requests/121#note_119964168
   */


  if (CPC_check_AMSDOS(pal_file, NULL, &pal_size))
    fseek(pal_file, 128, SEEK_SET); // right after AMSDOS header
  else
  {
    pal_size = File_length_file(pal_file);
    fseek(pal_file, 0, SEEK_SET);
  }

  if (pal_size != 239)
  {
    fclose(pal_file);
    return;
  }

  if (!Read_byte(pal_file, &mode) || !Read_byte(pal_file, &color_anim_flag))
  {
    fclose(pal_file);
    return;
  }
  GFX2_Log(GFX2_DEBUG, "Test_SCR() mode=%d color animation flag %02X\n", mode, color_anim_flag);
  if (mode <= 2 && (color_anim_flag == 0 || color_anim_flag == 0xff))
    File_error = 0;
  fclose(pal_file);
}

/**
 * Load Advanced OCP Art Studio files (Amstrad CPC)
 *
 * Only standard resolution files (Mode 0 160x200, mode 1 320x200 and
 * mode 2 640x200) are supported. The .PAL file presence is required.
 * "MJH" RLE packing is supported.
 *
 * .WIN "window" format is also supported.
 *
 * @todo Ask user for screen size (or register values) in order to support
 * non standard resolutions.
 */
void Load_SCR(T_IO_Context * context)
{
    // The Amstrad CPC screen memory is mapped in a weird mode, somewhere
    // between bitmap and textmode. Basically the only way to decode this is to
    // emulate the video chip and read the bytes as needed...
    // Moreover, the hardware allows the screen to have any size from 8x1 to
    // 800x273 pixels, and there is no indication of that in the file besides
    // its size. It can also use any of the 3 screen modes. Fortunately this
    // last bit of information is stored in the palette file.
    // Oh, and BTW, the picture can be offset, and it's even usual to do it,
    // because letting 128 pixels unused at the beginning of the file make it a
    // lot easier to handle screens using more than 16K of VRam.
    // The pixel encoding change with the video mode so we have to know that
    // before attempting to load anything...
    // As if this wasn't enough, Advanced OCP Art Studio, the reference tool on
    // Amstrad, can use RLE packing when saving files, meaning we also have to
    // handle that.

    // All this mess enforces us to load (and unpack if needed) the file to a
    // temporary 32k buffer before actually decoding it.
  FILE * pal_file, * file;
  unsigned long real_file_size, file_size, amsdos_file_size = 0;
  word addr;
  word load_address = 0x4000; // default for OCP Art studio
  word display_start = 0x4000;
  byte mode, color_anim_flag, color_anim_delay;
  byte pal_data[236]; // 12 palettes of 16+1 colors + 16 excluded inks + 16 protected inks
  word width, height = 200;
  byte bpp;
  enum PIXEL_RATIO ratio;
  byte * cpc_ram;
  word x, y;
  int i;
  byte sig[3];
  word block_length;
  word win_width, win_height;
  int is_win = 0;
  int columns = 80;
  int cpc_plus = 0;
  const byte * cpc_plus_pal = NULL;

  File_error = 1;
  // requires the PAL file for OCP Art studio files
  pal_file = Open_file_read_with_alternate_ext(context, "pal");
  if (pal_file != NULL)
  {
    file_size = File_length_file(pal_file);
    if (CPC_check_AMSDOS(pal_file, NULL, &file_size))
      fseek(pal_file, 128, SEEK_SET); // right after AMSDOS header
    else
      fseek(pal_file, 0, SEEK_SET);
    if (!Read_byte(pal_file, &mode) || !Read_byte(pal_file, &color_anim_flag)
          || !Read_byte(pal_file, &color_anim_delay) || !Read_bytes(pal_file, pal_data, 236))
    {
      GFX2_Log(GFX2_WARNING, "Load_SCR() failed to load .PAL file\n");
      fclose(pal_file);
      return;
    }
    fclose(pal_file);
    GFX2_Log(GFX2_DEBUG, "Load_SCR() mode=%d color animation flag=%02X delay=%u\n",
             mode, color_anim_flag, color_anim_delay);
  }

  file = Open_file_read(context);
  if (file == NULL)
    return;
  file_size = File_length_file(file);
  real_file_size = file_size;
  if (CPC_check_AMSDOS(file, &load_address, &amsdos_file_size))
  {
    display_start = load_address;
    if (file_size < (amsdos_file_size + 128))
    {
      GFX2_Log(GFX2_ERROR, "Load_SCR() mismatch in file size. AMSDOS file size %lu, should be %lu\n", amsdos_file_size, file_size - 128);
      fclose(file);
      return;
    }
    else if (file_size > (amsdos_file_size + 128))
      GFX2_Log(GFX2_INFO, "Load_SCR() %lu extra bytes at end of file\n", file_size - 128 - amsdos_file_size);
    fseek(file, 128, SEEK_SET); // right after AMSDOS header
    file_size = amsdos_file_size;
  }
  else
    fseek(file, 0, SEEK_SET);

  if (!Read_bytes(file, sig, 3) || !Read_word_le(file, &block_length))
  {
    fclose(file);
    return;
  }
  fseek(file, -5, SEEK_CUR);

  cpc_ram = GFX2_malloc(64*1024);
  memset(cpc_ram, 0, 64*1024);

  if (0 != memcmp(sig, "MJH", 3) || block_length > 16384)
  {
    // raw data
    Read_bytes(file, cpc_ram + load_address, file_size);
    i = file_size;
  }
  else
  {
    // MJH packed format
    i = 0;
    do
    {
      if (!Read_bytes(file, sig, 3) || !Read_word_le(file, &block_length))
        break;
      if (0 != memcmp(sig, "MJH", 3))
        break;
      GFX2_Log(GFX2_DEBUG, "  %.3s block %u\n", sig, block_length);
      file_size -= 5;
      while (block_length > 0)
      {
        byte code;
        if (!Read_byte(file, &code))
          break;
        file_size--;
        if (code == 1)
        {
          byte repeat, value;
          if (!Read_byte(file, &repeat) || !Read_byte(file, &value))
            break;
          file_size -= 2;
          do
          {
            cpc_ram[load_address + i++] = value;
            block_length--;
          }
          while(--repeat != 0);
        }
        else
        {
          cpc_ram[load_address + i++] = code;
          block_length--;
        }
      }
      GFX2_Log(GFX2_DEBUG, "  unpacked %d bytes. remaining bytes in file=%lu\n",
               i, file_size);
    }
    while(file_size > 0 && i < 16384);
  }
  fclose(file);

  if (i > 5)
  {
    win_width = cpc_ram[load_address + i - 4] + (cpc_ram[load_address + i - 3] << 8);  // in bits
    win_height = cpc_ram[load_address + i - 2];
    if (((win_width + 7) >> 3) * win_height + 5 == i) // that's a WIN file !
    {
      width = win_width >> (2 - mode);
      height = win_height;
      is_win = 1;
      columns = (win_width + 7) >> 3;
      GFX2_Log(GFX2_DEBUG, ".WIN file detected len=%d (%d,%d) %dcols %02X %02X %02X %02X %02X\n",
          i, width, height, columns,
          cpc_ram[load_address + i - 5], cpc_ram[load_address + i - 4], cpc_ram[load_address + i - 3],
          cpc_ram[load_address + i - 2], cpc_ram[load_address + i - 1]);
    }
    else
    {
      GFX2_Log(GFX2_DEBUG, ".SCR file. Data length %d\n", i);
      if (load_address == 0x170)
      {
        // fichier iMPdraw v2
        // http://orgams.wikidot.com/le-format-impdraw-v2
        GFX2_Log(GFX2_DEBUG, "Detected \"%s\"\n", cpc_ram + load_address + 6);
        mode = cpc_ram[load_address + 0x14] - 0x0e;
        cpc_plus = cpc_ram[load_address + 0x3c];
        GFX2_Log(GFX2_DEBUG, "Mode %d CPC %d\n", (int)mode, (int)cpc_plus);
        for (addr = load_address + 0x1d; cpc_ram[addr] < 16; addr += 2)
        {
          GFX2_Log(GFX2_DEBUG, " R%d = &H%02x = %d\n", cpc_ram[addr], cpc_ram[addr+1], cpc_ram[addr+1]);
          // see http://www.cpcwiki.eu/index.php/CRTC#The_6845_Registers
          switch(cpc_ram[addr])
          {
            case 1:
              columns = cpc_ram[addr+1] * 2;
              break;
            case 6:
              height = cpc_ram[addr+1] * 8;
              break;
            case 12:
              display_start = ((cpc_ram[addr+1] & 0x30) << 10) | ((cpc_ram[addr+1] & 0x03) << 9);
              GFX2_Log(GFX2_DEBUG, "  display_start &H%04X\n", display_start);
           }
        }
        snprintf(context->Comment, COMMENT_SIZE, "%s mode %d %s",
                 cpc_ram + load_address + 7, mode, cpc_plus ? "CPC+" : "");
        if (cpc_plus)
        {
          // palette at 0x801 (mode at 0x800 ?)
          GFX2_LogHexDump(GFX2_DEBUG, "", cpc_ram, 0x800, 0x21);
          cpc_plus_pal = cpc_ram + 0x801;
        }
        else
        {
          int j;
          // palette at 0x7f00
          GFX2_LogHexDump(GFX2_DEBUG, "", cpc_ram, 0x7f00, 16);
          for (j = 0; j < 16; j++)
            pal_data[12*j] = cpc_ram[0x7f00 + j];
        }
      }
      else if (load_address == 0x200)
      {
        /* from HARLEY.SCR :
        0800  00 = mode
        0801-0810 palette (Firmware colors)
        0811  21 47 08      LD HL,0847  ; OVERSCAN_REG_VALUES
        0814  cd 36 08      CALL 0836 ; LOAD_CRTC_REGS
        0817  3a 00 08      LD A,(0800) ; MODE
        081a  cd 1c bd      CALL BD1C ; Set screen mode
        081d  21 01 08      LD HL,0801  ; PALETTE
        0820  af            XOR A
            LOOP:
        0821  4e            LD C,(HL)
        0822  41            LD B,C
        0823  f5            PUSH AF
        0824  e5            PUSH HL
        0825  cd 32 bc      CALL BC32   ; SET ink A to color B,C
        0828  e1            POP HL
        0829  f1            POP AF
        082a  23            INC HL
        082b  3c            INC A
        082c  fe 10         CMP 10
        082e  20 f1         JR NZ,0821  ; LOOP
        0830  cd 18 bb      CALL BB18 ; Wait key press
        0833  21 55 08      LD HL,0855  ; STANDARD_REG_VALUES
            LOAD_CRTC_REGS:
        0836  01 00 bc      LD BC,BC00
            LOOP_CRTC:
        0839  7e            LD A,(HL)
        083a  a7            AND A
        083b  c8            RET Z
        083c  ed 79         OUT (C),A
        083e  04            INC B
        083f  23            INC HL
        0840  7e            LD A,(HL)
        0841  ed 79         OUT (C),A
        0843  23            INC HL
        0844  05            DEC B
        0845  18 f2         JR 0839 ; LOOP_CRTC
            OVERSCAN_REG_VALUES:
        0847  01 30  02 32  06 22  07 23  0c 0d  0d 00  00 00
            STANDARD_REG_VALUES:
        0855  01 28  02 2e  06 19  07 1e  0c 30  00 00
        */
        int j;
        static const byte CPC_Firmware_Colors[] = {
          0x54, 0x44, 0x55, 0x5c, 0x58, 0x5d, 0x4c, 0x45, 0x4d,
          0x56, 0x46, 0x57, 0x5e, 0x40, 0x5f, 0x4e, 0x47, 0x4f,
          0x52, 0x42, 0x53, 0x5a, 0x59, 0x5b, 0x4a, 0x43, 0x4b };
        mode = cpc_ram[0x800];
        for (j = 0; j < 16; j++)
          pal_data[12*j] = CPC_Firmware_Colors[cpc_ram[0x801 + j]];
        addr = 0x847;
        if (cpc_ram[0x80bb] == 1)
          addr = 0x80bb;
        for (; cpc_ram[addr] > 0 && cpc_ram[addr] < 16; addr += 2)
        {
          GFX2_Log(GFX2_DEBUG, " R%d = &H%02x = %d\n", cpc_ram[addr], cpc_ram[addr+1], cpc_ram[addr+1]);
          // see http://www.cpcwiki.eu/index.php/CRTC#The_6845_Registers
          switch(cpc_ram[addr])
          {
            case 1:
              columns = cpc_ram[addr+1] * 2;
              break;
            case 6:
              height = cpc_ram[addr+1] * 8;
              break;
            case 12:
              display_start = (display_start & 0x00ff) | ((cpc_ram[addr+1] & 0x30) << 10) | ((cpc_ram[addr+1] & 0x03) << 9);
              break;
            case 13:
              display_start = (display_start & 0xff00) | cpc_ram[addr+1];
           }
         }
      }
      if (i >= 30000)
      {
        height = 272; columns = 96;
      }
    }
  }

  switch (mode)
  {
    case 0:
      width = columns * 2;
      bpp = 4;
      ratio = PIXEL_WIDE;
      break;
    case 1:
      width = columns * 4;
      bpp = 2;
      ratio = PIXEL_SIMPLE;
      break;
    case 2:
      width = columns * 8;
      bpp = 1;
      ratio = PIXEL_TALL;
      break;
    default:
      return; // unsupported
  }

  if (Config.Clear_palette)
    memset(context->Palette,0,sizeof(T_Palette));
  // Setup the palette (amstrad hardware palette)
  CPC_set_HW_palette(context->Palette + 0x40);

  // Set the palette for this picture
  if (cpc_plus_pal)
  {
    for (i = 0; i < 16; i++)
    {
      context->Palette[i].G = cpc_plus_pal[i*2 + 1] * 0x11;
      context->Palette[i].R = (cpc_plus_pal[i*2] >> 4) * 0x11;
      context->Palette[i].B = (cpc_plus_pal[i*2] & 15) * 0x11;
    }
  }
  else
  {
    for (i = 0; i < 16; i++)
      context->Palette[i] = context->Palette[pal_data[12*i]];
  }

  File_error = 0;
  Pre_load(context, width, height, real_file_size, FORMAT_SCR, ratio, bpp);

  if (!is_win)
  {
    // Standard resolution files have the 200 lines stored in block
    // of 25 lines of 80 bytes = 2000 bytes every 2048 bytes.
    // so there are 48 bytes unused every 2048 bytes...
    for (y = 0; y < 8; y++)
    {
      addr = display_start + 0x800 * y;
      if (y > 0 && (display_start & 0x7ff))
      {
        if (!GFX2_is_mem_filled_with(cpc_ram + (addr & 0xf800), 0, display_start & 0x7ff))
          GFX2_LogHexDump(GFX2_DEBUG, "SCR1 ", cpc_ram,
                          addr & 0xf800, display_start & 0x7ff);
      }
      addr += (height >> 3) * columns;
      block_length = (height >> 3) * columns + (display_start & 0x7ff);
      if (block_length <= 0x800)
      {
        block_length = 0x800 - block_length;
        if (!GFX2_is_mem_filled_with(cpc_ram + addr, 0, block_length))
          GFX2_LogHexDump(GFX2_DEBUG, "SCR2 ", cpc_ram,
                          addr, block_length);
      }
      else
      {
        block_length = 0x1000 - block_length;
        if (!GFX2_is_mem_filled_with(cpc_ram + addr + 0x4000, 0, block_length))
          GFX2_LogHexDump(GFX2_DEBUG, "SCR2 ", cpc_ram,
                          addr + 0x4000, block_length);
      }
    }
    //for (j = 0; j < i; j += 2048)
    //  GFX2_LogHexDump(GFX2_DEBUG, "SCR ", cpc_ram, load_address + j + 2000, 48);
  }

  GFX2_Log(GFX2_DEBUG, "  display_start &H%04X\n", display_start);
  for (y = 0; y < height; y++)
  {
    const byte * line;

    if (is_win)
      addr = display_start + y * columns;
    else
    {
      addr = display_start + ((y >> 3) * columns);
      addr = (addr & 0xC7FF) | ((addr & 0x800) << 3);
      addr += (y & 7) << 11;
    }
    //GFX2_Log(GFX2_DEBUG, "line#%d &H%04X\n", y, addr);
    line = cpc_ram + addr;
    x = 0;
    for (i = 0; i < columns; i++)
    {
      byte pixels = line[i];
      switch (mode)
      {
        case 0:
          Set_pixel(context, x++, y, (pixels & 0x80) >> 7 | (pixels & 0x08) >> 2 | (pixels & 0x20) >> 3 | (pixels & 0x02) << 2);
          Set_pixel(context, x++, y, (pixels & 0x40) >> 6 | (pixels & 0x04) >> 1 | (pixels & 0x10) >> 2 | (pixels & 0x01) << 3);
          break;
        case 1:
          do {
            // upper nibble is 4 lower color bits, lower nibble is 4 upper color bits
            Set_pixel(context, x++, y, (pixels & 0x80) >> 7 | (pixels & 0x08) >> 2);
            pixels <<= 1;
          }
          while ((x & 3) != 0);
          break;
        case 2:
          do {
            Set_pixel(context, x++, y, (pixels & 0x80) >> 7);
            pixels <<= 1;
          }
          while ((x & 7) != 0);
      }
    }
  }

  free(cpc_ram);
}

/**
 * Save Amstrad SCR file
 *
 * guess mode from aspect ratio :
 * - normal pixels are mode 1
 * - wide pixels are mode 0
 * - tall pixels are mode 2
 *
 * Mode and palette are stored in a .PAL file.
 *
 * The picture color index should be 0-15,
 * The CPC Hardware palette is expected to be set (indexes 64 to 95)
 *
 * @todo Add possibility to set R9, R12, R13 values
 * @todo Add OCP packing support
 * @todo Add possibility to include AMSDOS header, with proper loading
 *       address guessed from r12/r13 values.
 */
void Save_SCR(T_IO_Context * context)
{
  int i, j;
  unsigned char* output;
  unsigned long outsize = 0;
  unsigned char r1 = 0;
  int cpc_mode;
  FILE* file;


  switch(Pixel_ratio)
  {
    case PIXEL_WIDE:
    case PIXEL_WIDE2:
      cpc_mode = 0;
      break;
    case PIXEL_TALL:
    case PIXEL_TALL2:
    case PIXEL_TALL3:
      cpc_mode = 2;
      break;
    default:
      cpc_mode = 1;
      break;
  }

  file = Open_file_write_with_alternate_ext(context, "pal");
  if (file == NULL)
    return;
  if (!Write_byte(file, cpc_mode) || !Write_byte(file, 0) || !Write_byte(file, 0))
  {
    fclose(file);
    return;
  }
  for (i = 0; i < 16; i++)
  {
    // search for the color in the HW palette (0x40-0x5F)
    byte index = 0x40;
    while ((index < 0x60) &&
        !CPC_compare_colors(context->Palette + i, context->Palette + index))
      index++;
    if (index >= 0x60)
    {
      GFX2_Log(GFX2_WARNING, "Save_SCR() color #%i not found in CPC HW palette.\n", i);
      index = 0x54 - i; // default
    }
    for (j = 0; j < 12; j++)  // write the same color for the 12 frames
    {
      Write_byte(file, index);
    }
  }
  // border
  for (j = 0; j < 12; j++)
  {
    Write_byte(file, 0x54); // black
  }
  // excluded inks
  for (i = 0; i < 16; i++)
  {
    Write_byte(file, 0);
  }
  // protected inks
  for (i = 0; i < 16; i++)
  {
    Write_byte(file, 0);
  }
  fclose(file);

  output = raw2crtc(context, cpc_mode, 7, &outsize, &r1, 0x0C, 0);
  GFX2_Log(GFX2_DEBUG, "Save_SCR() output=%p outsize=%lu r1=$%02X\n", output, outsize, r1);

  if (output == NULL)
    return;

  file = Open_file_write(context);
  if (file == NULL)
    File_error = 1;
  else
  {
    File_error = 0;
    if (!Write_bytes(file, output, outsize))
      File_error = 1;
    fclose(file);
  }
  free (output);
}

/**
 * Test for GO1/GO2/KIT - Amstrad Plus Graphos
 *
 * This format is made of 3 files
 * .KIT hold the palette in "Kit4096" format. There are 16 colors each stored
 * as 12 bit RGB in RB0G order.
 * .GO1 and GO2 hold each half of the picture (top and bottom)
 * The file always cover the whole display of the Plus (196*272 or so)
 */
void Test_GOS(T_IO_Context * context, FILE * file)
{
  FILE *file_oddeve;
  unsigned long file_size = 0;

  if (!CPC_check_AMSDOS(file, NULL, &file_size))
    file_size = File_length_file(file);
  if (file_size < 16383 || file_size > 16384) {
    File_error = 1;
    return;
  }

  file_oddeve = Open_file_read_with_alternate_ext(context, "GO2");
  if (file_oddeve == NULL) {
    File_error = 2;
    return;
  }
  if (!CPC_check_AMSDOS(file_oddeve, NULL, &file_size))
    file_size = File_length_file(file_oddeve);
  fclose(file_oddeve);
  if (file_size < 16383 || file_size > 16384) {
    File_error = 3;
    return;
  }

  File_error = 0;
}


/**
 * Load GO1/GO2/KIT - Amstrad CPC Plus Graphos
 */
void Load_GOS(T_IO_Context* context)
{
  FILE *file;
  unsigned long file_size;
  int i;
  int x, y;
  byte * pixel_data;

  if (!(file = Open_file_read(context)))
  {
      File_error = 1;
      return;
  }

  if (CPC_check_AMSDOS(file, NULL, &file_size))
    fseek(file, 128, SEEK_SET); // right after AMSDOS header
  else
    file_size = File_length_file(file);

  context->Ratio = PIXEL_WIDE;
  Pre_load(context, 192, 272, file_size, FORMAT_GOS, context->Ratio, 0);
  context->Width = 192;
  context->Height = 272;

  // load pixels
  pixel_data = GFX2_malloc(16384);
  memset(pixel_data, 0, 16384);
  Read_bytes(file, pixel_data, file_size);

  i = 0;
  for (y = 0; y < 168; y++) {
    x = 0;
    while (x < 192) {
      byte pixels = pixel_data[i];
      Set_pixel(context, x++, y, (pixels & 0x80) >> 7 | (pixels & 0x08) >> 2 | (pixels & 0x20) >> 3 | (pixels & 0x02) << 2);
      Set_pixel(context, x++, y, (pixels & 0x40) >> 6 | (pixels & 0x04) >> 1 | (pixels & 0x10) >> 2 | (pixels & 0x01) << 3);
      i++;
    }

    i += 0x800;
    if (i > 0x3FFF) {
      i -= 0x4000;
    } else {
      i -= 192 / 2;
    }
  }

  fclose(file);

  // load pixels from GO2
  file = Open_file_read_with_alternate_ext(context, "GO2");
  if (CPC_check_AMSDOS(file, NULL, &file_size))
    fseek(file, 128, SEEK_SET); // right after AMSDOS header

  Read_bytes(file, pixel_data, file_size);
  i = 0;
  for (y = 168; y < 272; y++) {
    x = 0;
    while (x < 192) {
      byte pixels = pixel_data[i];
      Set_pixel(context, x++, y, (pixels & 0x80) >> 7 | (pixels & 0x08) >> 2 | (pixels & 0x20) >> 3 | (pixels & 0x02) << 2);
      Set_pixel(context, x++, y, (pixels & 0x40) >> 6 | (pixels & 0x04) >> 1 | (pixels & 0x10) >> 2 | (pixels & 0x01) << 3);
      i++;
    }

    i += 0x800;
    if (i > 0x3FFF) {
      i -= 0x4000;
    } else {
      i -= 192 / 2;
    }
  }

  fclose(file);

  file = Open_file_read_with_alternate_ext(context, "KIT");
  if (file == NULL) {
    // There is no palette, but that's fine, we can still load the pixels
    return;
  }

  if (CPC_check_AMSDOS(file, NULL, &file_size)) {
    fseek(file, 128, SEEK_SET); // right after AMSDOS header
  } else {
    file_size = File_length_file(file);
  }

  if (Config.Clear_palette)
    memset(context->Palette,0,sizeof(T_Palette));

  File_error = 0;

  if (file_size == 32)
  {
    for (i = 0; i < 16; i++)
    {
      uint16_t word;
      if (!Read_word_le(file, &word))
      {
        File_error = 2;
        return;
      }

      context->Palette[i].R = ((word >>  4) & 0xF) * 0x11;
      context->Palette[i].G = ((word >>  8) & 0xF) * 0x11;
      context->Palette[i].B = ((word >>  0) & 0xF) * 0x11;
    }
  }
  else
  {
    // Setup the palette (amstrad hardware palette)
    CPC_set_HW_palette(context->Palette + 0x40);
    for (i = 0; i < 16; i++)
    {
      byte ink;
      if (!Read_byte(file, &ink))
      {
        File_error = 2;
        return;
      }
      context->Palette[i] = context->Palette[ink];
    }
  }

  fclose(file);
}

/**
 * Test for CM5 - Amstrad CPC "Mode 5" picture
 *
 * This is a format designed by SyX.
 * There is one .GFX file in the usual amstrad format
 * and a .CM5 file with the palette, which varies over time.
 *
 * CM5 file is 2049 bytes, GFX is 18432 bytes.
 *
 * @todo check CM5 contains only valid values [0x40-0x5f]
 */
void Test_CM5(T_IO_Context * context, FILE * file)
{
  // check cm5 file size == 2049 bytes
  FILE *file_gfx;
  long file_size;

  File_error = 1;

  file_size = File_length_file(file);
  if (file_size != 2049)
    return;

  // check existence of a .GFX file with the same name
  file_gfx = Open_file_read_with_alternate_ext(context, "gfx");
  if (file_gfx == NULL)
    return;
  file_size = File_length_file(file_gfx);
  fclose(file_gfx);
  if (file_size != 18432)
    return;

  File_error = 0;
}


/**
 * Load Amstrad CPC "Mode 5" picture
 *
 * Only support 288x256 resolution as the Mode 5 Viewer app only handles this
 * single resoltion.
 */
void Load_CM5(T_IO_Context* context)
{
  // Ensure "8bit" constraint mode is switched on
  // Set palette to the CPC hardware colors
  // Load the palette data to the 4 colorlayers
  FILE *file;
  byte value = 0;
  int mod=0;
  short line = 0;
  int tx, ty;
  // for preview :
  byte ink0;
  byte ink1[256];
  byte ink2[256];
  byte ink3[256*6];

  if (!(file = Open_file_read(context)))
  {
      File_error = 1;
      return;
  }

  Pre_load(context, 48*6, 256, 2049, FORMAT_CM5, PIXEL_SIMPLE, 0);

  if (Config.Clear_palette)
  {
    memset(context->Palette,0,sizeof(T_Palette));
    // setup colors 0,1,2,3 to see something in the thumbnail preview of layer 5
    context->Palette[1].R = 60;
    context->Palette[2].B = 60;
    context->Palette[3].G = 60;
  }

  // Setup the palette (amstrad hardware palette)
  CPC_set_HW_palette(context->Palette + 0x40);

  First_color_in_palette = 64;

  if (!Read_byte(file, &ink0))
    File_error = 2;

  // This forces the creation of 5 layers total :
  // Needed because the "pixel" functions will seek layer 4
  Set_loading_layer(context, 4);
  // Now select layer 1 again
  Set_loading_layer(context, 0);

  if (context->Type == CONTEXT_MAIN_IMAGE)
  {
    Set_image_mode(context, IMAGE_MODE_MODE5);

    // Fill layer with color we just read (Layer 1 - INK 0)
    for(ty=0; ty<context->Height; ty++)
      for(tx=0; tx<context->Width; tx++)
        Set_pixel(context, tx, ty, ink0);
  }

  while(Read_byte(file, &value))
  {
    switch(mod)
    {
      case 0:
        // This is color for layer 2 - INK 1
        Set_loading_layer(context, 1);
        for(tx=0; tx<context->Width; tx++)
          Set_pixel(context, tx, line, value);
        ink1[line] = value;
        break;
      case 1:
        // This is color for layer 3 - INK 2
        Set_loading_layer(context, 2);
        for(tx=0; tx<context->Width; tx++)
          Set_pixel(context, tx, line, value);
        ink2[line] = value;
        break;
      default:
        // This is color for a block in layer 4 - INK 3
        Set_loading_layer(context, 3);
        for(tx=(mod-2)*48; tx<(mod-1)*48; tx++)
          Set_pixel(context, tx, line, value);
        ink3[line*6+(mod-2)] = value;
        break;
    }
    mod++;
    if (mod > 7)
    {
      mod = 0;
      line++;
    }
  }

  fclose(file);

  // Load the pixeldata to the 5th layer
  file = Open_file_read_with_alternate_ext(context, "gfx");
  if (file == NULL)
  {
    File_error = 1;
    return;
  }
  Set_loading_layer(context, 4);

  if (context->Type == CONTEXT_PREVIEW)
    for (ty = 0; ty < 256; ty++)
      for (tx = 0; tx < 48*6; )
      {
        Read_byte(file, &value);
        for (mod = 0; mod < 4; mod++, tx++, value <<= 1)
        {
          switch(3 ^ (((value&0x80) >> 7) | ((value&0x8)>>2)))  // INK
          {
            case 0:
              Set_pixel(context, tx, ty, ink0);
              break;
            case 1:
              Set_pixel(context, tx, ty, ink1[ty]);
              break;
            case 2:
              Set_pixel(context, tx, ty, ink2[ty]);
              break;
            default:
              Set_pixel(context, tx, ty, ink3[ty*6+(tx/48)]);
          }
        }
      }
  else
    for (ty = 0; ty < 256; ty++)
      for (tx = 0; tx < 48*6; )
      {
        Read_byte(file, &value);
        Set_pixel(context, tx++, ty, 3 ^ (((value&0x80) >> 7) | ((value&0x8)>>2)));
        Set_pixel(context, tx++, ty, 3 ^ (((value&0x40) >> 6) | ((value&0x4)>>1)));
        Set_pixel(context, tx++, ty, 3 ^ (((value&0x20) >> 5) | ((value&0x2)>>0)));
        Set_pixel(context, tx++, ty, 3 ^ (((value&0x10) >> 4) | ((value&0x1)<<1)));
      }

  fclose(file);

}


void Save_CM5(T_IO_Context* context)
{
  FILE* file;
  int tx, ty;

  // TODO: Check picture has 5 layers
  // TODO: Check the constraints on the layers
  // Layer 1 : 1 color Only
  // Layer 2 and 3 : 1 color/line
  // Layer 4 : 1 color / 48x1 block
  // TODO: handle filesize

  if (!(file = Open_file_write(context)))
  {
    File_error = 1;
    return;
  }
  setvbuf(file, NULL, _IOFBF, 64*1024);

  // Write layer 0
  Set_saving_layer(context, 0);
  Write_byte(file, Get_pixel(context, 0, 0));
  for(ty = 0; ty < 256; ty++)
  {
    Set_saving_layer(context, 1);
    Write_byte(file, Get_pixel(context, 0, ty));
    Set_saving_layer(context, 2);
    Write_byte(file, Get_pixel(context, 0, ty));
    Set_saving_layer(context, 3);
    for(tx = 0; tx < 6; tx++)
    {
      Write_byte(file, Get_pixel(context, tx*48, ty));
    }
  }

  fclose(file);

  // Now the pixeldata
  if (!(file = Open_file_write_with_alternate_ext(context, "gfx")))
  {
    File_error = 2;
    return;
  }
  setvbuf(file, NULL, _IOFBF, 64*1024);

  Set_saving_layer(context, 4);

  for (ty = 0; ty < 256; ty++)
  {
    for (tx = 0; tx < 48*6; tx+=4)
    {
      byte code = 0;
      byte pixel;

      pixel = 3-Get_pixel(context, tx+3, ty);
      code |= (pixel&2)>>1 | ((pixel & 1)<<4);
      pixel = 3-Get_pixel(context, tx+2, ty);
      code |= ((pixel&2)<<0) | ((pixel & 1)<<5);
      pixel = 3-Get_pixel(context, tx+1, ty);
      code |= ((pixel&2)<<1) | ((pixel & 1)<<6);
      pixel = 3-Get_pixel(context, tx, ty);
      code |= ((pixel&2)<<2) | ((pixel & 1)<<7);
      Write_byte(file, code);
    }
  }

  fclose(file);
  File_error = 0;

}


/* Amstrad CPC 'PPH' for Perfect Pix.
// This is a format designed by Rhino.
// There are 3 modes:
// - Mode 'R': 1:1 pixels, 16 colors from the CPC 27 color palette.
//   (this is implemented on CPC as two pictures with wide pixels, the "odd" one
//   being shifted half a pixel to the right), and flipping)
// - Mode 'B0': wide pixels, up to 126 out of 378 colors.
//   (this is implemented as two pictures with wide pixels, sharing the same 16
//   color palette, and flipping)
// - Mode 'B1': 1:1 pixels, 1 fixed color, up to 34 palettes of 9 colors
//   (actually 4 colors + flipping)
//
// - The standard CPC formats can also be encapsulated into a PPH file.
//
// http://www.pouet.net/prod.php?which=67770#c766959
*/
void Test_PPH(T_IO_Context * context, FILE * file)
{
  FILE *file_oddeve;
  byte buffer[6];
  unsigned long file_size;
  unsigned int w, h;
  unsigned int expected;

  File_error = 1;

  // First check file size is large enough to hold the header
  file_size = File_length_file(file);
  if (file_size < 11) {
    File_error = 1;
    return;
  }

  // File is large enough for the header, now check if the data makes some sense
  if (!Read_bytes(file, buffer, 6))
    return;
  if (buffer[0] > 5) {
    // Unknown mode
    File_error = 2;
    return;
  }

  w = buffer[1] | (buffer[2] << 8);
  if (w < 2 || w > 384) {
    // Invalid width
    File_error = 3;
    return;
  }

  h = buffer[3] | (buffer[4] << 8);
  if (h < 1 || h > 272) {
    // Invalid height
    File_error = 4;
    return;
  }

  if (buffer[5] < 1 || buffer[5] > 28)
  {
    // Invalid palettes count
    File_error = 5;
    return;
  }
  expected = 6; // Size of header
  switch(buffer[0])
  {
    case 0:
    case 3:
    case 4:
      // Palette size should be 16 bytes, only 1 palette.
      if (buffer[5] != 1) {
        File_error = 7;
        return;
      }
      expected += 16;
      break;

    case 1:
    case 5:
      expected += buffer[5] * 5 - 1;
      break;

    case 2:
      // Palette size should be 2 bytes
      if (buffer[5] != 1) {
        File_error = 7;
        return;
      }
      expected += 2;
      break;
  }

  if (file_size != expected)
  {
    File_error = 6;
    return;
  }

  // check existence of .ODD/.EVE files with the same name
  // and the right size
  expected = w * h / 4;
  file_oddeve = Open_file_read_with_alternate_ext(context, "odd");
  if (file_oddeve == NULL)
    return;
  file_size = File_length_file(file_oddeve);
  fclose (file_oddeve);
  if (file_size != expected)
  {
    File_error = 8;
    return;
  }
  file_oddeve = Open_file_read_with_alternate_ext(context, "eve");
  if (file_oddeve == NULL)
    return;
  file_size = File_length_file(file_oddeve);
  fclose(file_oddeve);
  if (file_size != expected)
  {
    File_error = 8;
    return;
  }
  File_error = 0;
}


static uint8_t pph_blend(uint8_t a, uint8_t b)
{
	uint32_t h,l;
	if (a > b) { h = a; l = b; }
	else       { h = b; l = a; }

	return (23 * h + 9 * l) / 32;
}


void Load_PPH(T_IO_Context* context)
{
  FILE *file;
  FILE *feven;

  // Read in the header
  uint8_t mode;
  uint16_t width;
  uint16_t height;
  uint8_t npal;
  int i,j;
  uint8_t a,b,c,d;
  int file_size;
  uint8_t pl[16];

  static const T_Components CPCPAL[27] =
  {
      { 0x00, 0x02, 0x01 }, { 0x00, 0x02, 0x6B }, { 0x0C, 0x02, 0xF4 },
      { 0x6C, 0x02, 0x01 }, { 0x69, 0x02, 0x68 }, { 0x6C, 0x02, 0xF2 },
      { 0xF3, 0x05, 0x06 }, { 0xF0, 0x02, 0x68 }, { 0xF3, 0x02, 0xF4 },
      { 0x02, 0x78, 0x01 }, { 0x00, 0x78, 0x68 }, { 0x0C, 0x7B, 0xF4 },
      { 0x6E, 0x7B, 0x01 }, { 0x6E, 0x7D, 0x6B }, { 0x6E, 0x7B, 0xF6 },
      { 0xF3, 0x7D, 0x0D }, { 0xF3, 0x7D, 0x6B }, { 0xFA, 0x80, 0xF9 },
      { 0x02, 0xF0, 0x01 }, { 0x00, 0xF3, 0x6B }, { 0x0F, 0xF3, 0xF2 },
      { 0x71, 0xF5, 0x04 }, { 0x71, 0xF3, 0x6B }, { 0x71, 0xF3, 0xF4 },
      { 0xF3, 0xF3, 0x0D }, { 0xF3, 0xF3, 0x6D }, { 0xFF, 0xF3, 0xF9 }
  };

  if (!(file = Open_file_read(context)))
  {
      File_error = 1;
      return;
  }

  file_size=File_length_file(file);

  Read_byte(file, &mode);
  Read_word_le(file, &width);
  Read_word_le(file, &height);
  Read_byte(file, &npal);

  if (npal > 16)
      npal = 16;

  // Switch to the proper aspect ratio
  switch (mode)
  {
      case 0:
      case 4:
        context->Ratio = PIXEL_WIDE;
        width /= 2;
        break;

      case 2:
        context->Ratio = PIXEL_TALL;
        break;

      case 1:
      case 5:
      case 3:
        context->Ratio = PIXEL_SIMPLE;
        break;
  }

  Pre_load(context, width, height, file_size, FORMAT_PPH, context->Ratio, 0);

  context->Width = width;
  context->Height = height;

  // First of all, detect the mode
  // 0, 1, 2 > Load as with SCR files?
  // R(3)    > Load as single layer, square pixels, 16 colors
  // B0(4)   > Load as single layer, wide pixels, expand palette with colorcycling
  // B1(5)   > Load as ???
  //           Maybe special mode similar to mode5, with 2 layers + auto-flicker?

  switch (mode)
  {
      case 0:
      case 3: // R
          // 16-color palette
          for (i = 0; i < 16; i++)
          {
              uint8_t color;
              Read_byte(file, &color);
              context->Palette[i] = CPCPAL[color];
          }
          break;

      case 1:
      case 5: // B1
      {
          // Single or multiple 4-color palettes
          uint8_t base[4];
          for (j = 0; j < npal; j++)
          {
            for (i = 0; i < 4; i++)
            {
              Read_byte(file,&base[i]);
            }
            for (i = 0; i < 16; i++)
            {
              context->Palette[i + 16*j].R = pph_blend(
                  CPCPAL[base[i & 3]].R, CPCPAL[base[i >> 2]].R);
              context->Palette[i + 16*j].G = pph_blend(
                  CPCPAL[base[i & 3]].G, CPCPAL[base[i >> 2]].G);
              context->Palette[i + 16*j].B = pph_blend(
                  CPCPAL[base[i & 3]].B, CPCPAL[base[i >> 2]].B);
            }
            // TODO this byte marks where this palette stops being used and the
            // next starts. We must handle this!
            Read_byte(file,&pl[j]);
          }
          pl[npal - 1] = 255;
          break;
      }

      case 2:
          // Single 2-color palette
          break;

      case 4: // B0
      {
          // Single 16-color palette + flipping, need to expand palette and
          // setup colorcycling ranges.
          uint8_t base[16];
          for (i = 0; i < 16; i++)
          {
              Read_byte(file,&base[i]);
          }

          for (i = 0; i < 256; i++)
          {
              context->Palette[i].R = pph_blend(
                  CPCPAL[base[i & 15]].R, CPCPAL[base[i >> 4]].R);
              context->Palette[i].G = pph_blend(
                  CPCPAL[base[i & 15]].G, CPCPAL[base[i >> 4]].G);
              context->Palette[i].B = pph_blend(
                  CPCPAL[base[i & 15]].B, CPCPAL[base[i >> 4]].B);
          }
      }
      break;
  }

  fclose(file);

  // Load the picture data
  // There are two pages, each storing bytes in the CPC vram format but lines in
  // linear order.
  file = Open_file_read_with_alternate_ext(context, "odd");
  if (file == NULL)
  {
    File_error = 3;
    return;
  }
  feven = Open_file_read_with_alternate_ext(context, "eve");
  if (feven == NULL)
  {
    File_error = 4;
    fclose(file);
    return;
  }

  c = 0;
  d = 0;

  for (j = 0; j < height; j++)
  {
      for (i = 0; i < width;)
      {
          uint8_t even, odd;
          Read_byte(feven, &even);
          Read_byte(file, &odd);

          switch (mode)
          {
              case 4:
                a = ((even & 0x02) << 2) | ((even & 0x08) >> 2)
                  | ((even & 0x20) >> 3) | ((even & 0x80) >> 7);
                a <<= 4;
                a |= ((odd & 0x02) << 2) | (( odd & 0x08) >> 2)
                  | (( odd & 0x20) >> 3) | (( odd & 0x80) >> 7);

                b = ((even & 0x01) << 3) | ((even & 0x04) >> 1)
                  | ((even & 0x10) >> 2) | ((even & 0x40) >> 6);
                b <<= 4;
                b |= ((odd & 0x01) << 3) | (( odd & 0x04) >> 1)
                  | (( odd & 0x10) >> 2) | (( odd & 0x40) >> 6);

                Set_pixel(context, i++, j, a);
                Set_pixel(context, i++, j, b);
                break;

              case 3:
                a = ((even & 0x02) << 2) | ((even & 0x08) >> 2)
                  | ((even & 0x20) >> 3) | ((even & 0x80) >> 7);
                b = (( odd & 0x02) << 2) | (( odd & 0x08) >> 2)
                  | (( odd & 0x20) >> 3) | (( odd & 0x80) >> 7);
                c = ((even & 0x01) << 3) | ((even & 0x04) >> 1)
                  | ((even & 0x10) >> 2) | ((even & 0x40) >> 6);
                d = (( odd & 0x01) << 3) | (( odd & 0x04) >> 1)
                  | (( odd & 0x10) >> 2) | (( odd & 0x40) >> 6);
                Set_pixel(context, i++, j, j & 1 ? b : a);
                Set_pixel(context, i++, j, j & 1 ? a : b);
                Set_pixel(context, i++, j, j & 1 ? d : c);
                Set_pixel(context, i++, j, j & 1 ? c : d);
                break;

              case 5:
                if (d >= pl[c])
                {
                    d = 0;
                    c++;
                }
                a = ((even & 0x80) >> 6) | ((even & 0x08) >> 3);
                b = (( odd & 0x80) >> 6) | (( odd & 0x08) >> 3);
                Set_pixel(context, i++, j, a + (b << 2) + c * 16);
                a = ((even & 0x40) >> 5) | ((even & 0x04) >> 2);
                b = (( odd & 0x40) >> 5) | (( odd & 0x04) >> 2);
                Set_pixel(context, i++, j, a + (b << 2) + c * 16);
                a = ((even & 0x20) >> 4) | ((even & 0x02) >> 1);
                b = (( odd & 0x20) >> 4) | (( odd & 0x02) >> 1);
                Set_pixel(context, i++, j, a + (b << 2) + c * 16);
                a = ((even & 0x10) >> 3) | ((even & 0x01) >> 0);
                b = (( odd & 0x10) >> 3) | (( odd & 0x01) >> 0);
                Set_pixel(context, i++, j, a + (b << 2) + c * 16);

                break;

              default:
                File_error = 2;
                return;
          }

      }
      d++;
  }
  fclose(file);
  fclose(feven);

  File_error = 0;
}

void Save_PPH(T_IO_Context* context)
{
  (void)context; // unused
    // TODO

    // Detect mode
    // Wide pixels => B0 (4)
    // Square pixels:
    // - 16 colors used => R
    // - more colors used => B1 (if <16 colors per line)

    // Check palette
    // B0: use diagonal: 0, 17, 34, ... (assume the other are mixes)
    // R: use 16 used colors (or 16 first?)
    // B1: find the 16 colors used in a line? Or assume they are in-order already?
}


/////////////////////////////////// FLI/FLC /////////////////////////////////
typedef struct {
  dword size;          /* Size of FLIC including this header */
  word  type;          /* File type 0xAF11, 0xAF12, 0xAF30, 0xAF44, ... */
  word  frames;        /* Number of frames in first segment */
  word  width;         /* FLIC width in pixels */
  word  height;        /* FLIC height in pixels */
  word  depth;         /* Bits per pixel (usually 8) */
  word  flags;         /* Set to zero or to three */
  dword speed;         /* Delay between frames */
  word  reserved1;     /* Set to zero */
  dword created;       /* Date of FLIC creation (FLC only) */
  dword creator;       /* Serial number or compiler id (FLC only) */
  dword updated;       /* Date of FLIC update (FLC only) */
  dword updater;       /* Serial number (FLC only), see creator */
  word  aspect_dx;     /* Width of square rectangle (FLC only) */
  word  aspect_dy;     /* Height of square rectangle (FLC only) */
  word  ext_flags;     /* EGI: flags for specific EGI extensions */
  word  keyframes;     /* EGI: key-image frequency */
  word  totalframes;   /* EGI: total number of frames (segments) */
  dword req_memory;    /* EGI: maximum chunk size (uncompressed) */
  word  max_regions;   /* EGI: max. number of regions in a CHK_REGION chunk */
  word  transp_num;    /* EGI: number of transparent levels */
  byte  reserved2[24]; /* Set to zero */
  dword oframe1;       /* Offset to frame 1 (FLC only) */
  dword oframe2;       /* Offset to frame 2 (FLC only) */
  byte  reserved3[40]; /* Set to zero */
} T_FLIC_Header;

static void Load_FLI_Header(FILE * file, T_FLIC_Header * header)
{
  if (!(Read_dword_le(file,&header->size)
      && Read_word_le(file,&header->type)
      && Read_word_le(file,&header->frames)
      && Read_word_le(file,&header->width)
      && Read_word_le(file,&header->height)
      && Read_word_le(file,&header->depth)
      && Read_word_le(file,&header->flags)
      && Read_dword_le(file,&header->speed)
      && Read_word_le(file,&header->reserved1)
      && Read_dword_le(file,&header->created)
      && Read_dword_le(file,&header->creator)
      && Read_dword_le(file,&header->updated)
      && Read_dword_le(file,&header->updater)
      && Read_word_le(file,&header->aspect_dx)
      && Read_word_le(file,&header->aspect_dy)
      && Read_word_le(file,&header->ext_flags)
      && Read_word_le(file,&header->keyframes)
      && Read_word_le(file,&header->totalframes)
      && Read_dword_le(file,&header->req_memory)
      && Read_word_le(file,&header->max_regions)
      && Read_word_le(file,&header->transp_num)
      && Read_bytes(file,header->reserved2,24)
      && Read_dword_le(file,&header->oframe1)
      && Read_dword_le(file,&header->oframe2)
      && Read_bytes(file,header->reserved2,40) ))
  {
    File_error=1;
  }
}

/**
 * Test for the Autodesk Animator FLI/FLC format.
 *
 * Not to be confused with Commodore 64 FLI.
 */
void Test_FLI(T_IO_Context * context, FILE * file)
{
  T_FLIC_Header header;
  (void)context;

  File_error=0;
  Load_FLI_Header(file, &header);
  if (File_error != 0) return;

  switch (header.type)
  {
    case 0xAF11:  // standard FLI
    case 0xAF12:  // FLC (8bpp)
#if 0
    case 0xAF30:  // Huffman or BWT compression
    case 0xAF31:  // frame shift compression
    case 0xAF44:  // bpp != 8
#endif
      File_error=0;
      break;
    default:
      File_error=1;
  }
}

/**
 * Load file in the Autodesk Animator FLI/FLC format.
 *
 * Not to be confused with Commodore 64 FLI.
 */
void Load_FLI(T_IO_Context * context)
{
  FILE * file;
  unsigned long file_size;
  T_FLIC_Header header;
  dword chunk_size;
  word chunk_type;
  word sub_chunk_count, sub_chunk_index;
  dword sub_chunk_size;
  word sub_chunk_type;
  word frame_delay, frame_width, frame_height;
  int current_frame = 0;

  file = Open_file_read(context);
  if (file == NULL)
  {
    File_error=1;
    return;
  }
  File_error=0;
  file_size = File_length_file(file);
  Load_FLI_Header(file, &header);
  if (File_error != 0)
  {
    fclose(file);
    return;
  }
  if (header.size == 12)
  {
    // special "magic carpet" format
    header.depth = 8;
    header.speed = 66; // about 15fps
    fseek(file, 12, SEEK_SET);
  }
  else if (file_size != header.size)
    GFX2_Log(GFX2_WARNING, "Load_FLI(): file size mismatch in header %lu != %u\n", file_size, header.size);

  if (header.speed == 0)
  {
    if (header.type == 0xAF11) // FLI
      header.speed = 1;   // 1/70th seconds
    else
      header.speed = 10;  // 10ms
  }

  while (File_error == 0
     && Read_dword_le(file,&chunk_size) && Read_word_le(file,&chunk_type))
  {
    chunk_size -= 6;
    switch (chunk_type)
    {
      case 0xf1fa:  // FRAME
        Read_word_le(file, &sub_chunk_count);
        Read_word_le(file, &frame_delay);
        fseek(file, 2, SEEK_CUR);
        Read_word_le(file, &frame_width);
        Read_word_le(file, &frame_height);
        if (frame_width == 0)
          frame_width = header.width;
        if (frame_height == 0)
          frame_height = header.height;
        if (frame_delay == 0)
          frame_delay = header.speed;
        chunk_size -= 10;

        if (current_frame == 0)
        {
          Pre_load(context, header.width,header.height,file_size,FORMAT_FLI,PIXEL_SIMPLE,header.depth);
          Set_image_mode(context, IMAGE_MODE_ANIMATION);
          if (Config.Clear_palette)
            memset(context->Palette,0,sizeof(T_Palette));
        }
        else
        {
          Set_loading_layer(context, current_frame);
          if (context->Type == CONTEXT_MAIN_IMAGE && Get_image_mode(context) == IMAGE_MODE_ANIMATION)
          {
            // Copy the content of previous frame
            memcpy(
                Main.backups->Pages->Image[Main.current_layer].Pixels,
                Main.backups->Pages->Image[Main.current_layer-1].Pixels,
                Main.backups->Pages->Width*Main.backups->Pages->Height);
          }
        }
        if (header.type == 0xAF11) // FLI
          Set_frame_duration(context, (frame_delay * 100) / 7); // 1/70th sec
        else
          Set_frame_duration(context, frame_delay); // msec
        current_frame++;

        for (sub_chunk_index = 0; sub_chunk_index < sub_chunk_count; sub_chunk_index++)
        {
          if (!(Read_dword_le(file,&sub_chunk_size) && Read_word_le(file,&sub_chunk_type)))
            File_error = 1;
          else
          {
            chunk_size -= sub_chunk_size;
            sub_chunk_size -= 6;
            if (sub_chunk_type == 0x04 || sub_chunk_type == 0x0b)   // color map
            {
              word packet_count;
              int i = 0;
              sub_chunk_size -= 2;
              if (!Read_word_le(file, &packet_count))
                File_error = 1;
              else
                while (packet_count-- > 0 && File_error == 0)
                {
                  byte skip, count;
                  if (!(Read_byte(file, &skip) && Read_byte(file, &count)))
                    File_error = 1;
                  else
                  {
                    sub_chunk_size -= 2;
                    i += skip;  // count 0 means 256
                    do
                    {
                      byte r, g, b;
                      if (!(Read_byte(file, &r) && Read_byte(file, &g) && Read_byte(file, &b)))
                      {
                        File_error = 1;
                        break;
                      }
                      if (sub_chunk_type == 0x0b || header.size == 12) // 6bit per color
                      {
                        r = (r << 2) | (r >> 4);
                        g = (g << 2) | (g >> 4);
                        b = (b << 2) | (b >> 4);
                      }
                      context->Palette[i].R = r;
                      context->Palette[i].G = g;
                      context->Palette[i].B = b;
                      i++;
                      sub_chunk_size -= 3;
                    } while (--count != 0);
                  }
                }
            }
            else if (sub_chunk_type == 0x0f)  // full frame RLE
            {
              word x, y;
              for (y = 0; y < frame_height && File_error == 0; y++)
              {
                byte count, data;
                Read_byte(file, &count); // packet count, but dont rely on it
                sub_chunk_size--;
                for (x = 0; x < frame_width; )
                {
                  if (!Read_byte(file, &count))
                  {
                    File_error = 1;
                    break;
                  }
                  sub_chunk_size--;
                  if ((count & 0x80) == 0)
                  {
                    if (!Read_byte(file, &data))  // repeat data count times
                    {
                      File_error = 1;
                      break;
                    }
                    sub_chunk_size--;
                    while (count-- > 0 && x < frame_width)
                      Set_pixel(context, x++, y, data);
                  }
                  else
                    while (count++ != 0 && x < frame_width)  // copy count bytes
                    {
                      if (!Read_byte(file, &data))
                      {
                        File_error = 1;
                        break;
                      }
                      Set_pixel(context, x++, y, data);
                      sub_chunk_size--;
                    }
                }
              }
              if (context->Type == CONTEXT_PREVIEW || context->Type == CONTEXT_PREVIEW_PALETTE)
              { // load only 1st frame in preview
                fclose(file);
                return;
              }
            }
            else if (sub_chunk_type == 0x0c)  // delta image, RLE
            {
              word x, y, line_count;

              Read_word_le(file, &y);
              Read_word_le(file, &line_count);
              sub_chunk_size -= 4;
              while (sub_chunk_size > 0 && line_count > 0 && File_error == 0)
              {
                byte packet_count;

                x = 0;
                if (!Read_byte(file, &packet_count))
                  File_error = 1;
                else
                {
                  sub_chunk_size--;
                  while (packet_count-- > 0 && File_error == 0)
                  {
                    byte skip, count, data;
                    if (!(Read_byte(file, &skip) && Read_byte(file, &count)))
                      File_error = 1;
                    else
                    {
                      sub_chunk_size -= 2;
                      x += skip;
                      if (count & 0x80)
                      {
                        Read_byte(file, &data);
                        sub_chunk_size--;
                        while (count++ != 0)
                          Set_pixel(context, x++, y, data);
                      }
                      else
                        while (count-- > 0)
                        {
                          Read_byte(file, &data);
                          sub_chunk_size--;
                          Set_pixel(context, x++, y, data);
                        }
                    }
                  }
                }
                y++;
                line_count--;
              }
            }
            else if (sub_chunk_type == 0x07)  // FLC delta image
            {
              word opcode, y, line_count;

              y = 0;
              Read_word_le(file, &line_count);
              sub_chunk_size -= 2;
              while (line_count > 0)
              {
                Read_word_le(file, &opcode);
                sub_chunk_size -= 2;
                if ((opcode & 0xc000) == 0x0000) // packet count
                {
                  word x = 0;
                  while (opcode-- > 0)
                  {
                    byte skip, count, data1, data2;
                    if (!(Read_byte(file, &skip) && Read_byte(file, &count)))
                      File_error = 1;
                    else
                    {
                      sub_chunk_size -= 2;
                      x += skip;
                      if (count & 0x80)
                      {
                        Read_byte(file, &data1);
                        Read_byte(file, &data2);
                        sub_chunk_size -= 2;
                        while (count++ != 0)
                        {
                          Set_pixel(context, x++, y, data1);
                          Set_pixel(context, x++, y, data2);
                        }
                      }
                      else
                        while (count-- > 0)
                        {
                          Read_byte(file, &data1);
                          Set_pixel(context, x++, y, data1);
                          Read_byte(file, &data2);
                          Set_pixel(context, x++, y, data2);
                          sub_chunk_size -= 2;
                        }
                    }
                  }
                  y++;
                  line_count--;
                }
                else if ((opcode & 0xc000) == 0xc000)  // line skip
                {
                  y -= opcode;
                }
                else if ((opcode & 0xc000) == 0x8000)  // last byte
                {
                  Set_pixel(context, frame_width - 1, y, opcode & 0xff);
                }
                else
                {
                  GFX2_Log(GFX2_WARNING, "Unsupported opcode %04x\n", opcode);
                  File_error = 2;
                  break;
                }
              }
            }
            if (sub_chunk_size > 0)
            {
              fseek(file, sub_chunk_size, SEEK_CUR);
            }
          }
        }
        break;
      default:  // skip
        GFX2_Log(GFX2_WARNING, "Load_FLI(): unrecognized chunk %04x\n", chunk_type);
    }
    if (chunk_size > 0 && header.size != 12)
    {
      fseek(file, chunk_size, SEEK_CUR);
    }
  }
  fclose(file);
}

/////////////////////////////// Apple II Files //////////////////////////////

/**
 * Test for an Apple II HGR or DHGR raw file
 */
void Test_HGR(T_IO_Context * context, FILE * file)
{
  long file_size;

  (void)context;
  File_error = 1;

  file_size = File_length_file(file);
  if (file_size == 8192)  // HGR
    File_error = 0;
  else if(file_size == 16384) // DHGR
    File_error = 0;
}

/**
 * Load HGR (280x192) or DHGR (560x192) Apple II pictures
 *
 * Creates 2 layers :
 * 1. Monochrome
 * 2. Color
 */
void Load_HGR(T_IO_Context * context)
{
  unsigned long file_size;
  FILE * file;
  byte * vram[2];
  int bank;
  int x, y;
  int is_dhgr = 0;

  file = Open_file_read(context);
  if (file == NULL)
  {
    File_error = 1;
    return;
  }
  file_size = File_length_file(file);
  if (file_size == 16384)
    is_dhgr = 1;

  vram[0] = GFX2_malloc(8192);
  Read_bytes(file, vram[0], 8192);
  if (is_dhgr)
  {
    vram[1] = GFX2_malloc(8192);
    Read_bytes(file, vram[1], 8192);
  }
  else
    vram[1] = NULL;
  fclose(file);

  if (Config.Clear_palette)
    memset(context->Palette,0,sizeof(T_Palette));
  if (is_dhgr)
  {
    DHGR_set_palette(context->Palette);
    Pre_load(context, 560, 192, file_size, FORMAT_HGR, PIXEL_TALL, 4);
  }
  else
  {
    HGR_set_palette(context->Palette);
    Pre_load(context, 280, 192, file_size, FORMAT_HGR, PIXEL_SIMPLE, 2);
  }
  for (y = 0; y < 192; y++)
  {
    byte palette = 0, color = 0;
    byte previous_palette = 0;  // palette for the previous pixel pair
    int column, i;
    int offset = ((y & 7) << 10) + ((y & 070) << 4) + ((y >> 6) * 40);
    x = 0;
    for (column = 0; column < 40; column++)
    for (bank = 0; bank <= is_dhgr; bank++)
    {
      byte b = vram[bank][offset+column];
      if (!is_dhgr)
        palette = (b & 0x80) ? 4 : 0;
      else
        palette = (b & 0x80) ? 0 : 16;
      for (i = 0; i < 7; i++)
      {
        if (context->Type == CONTEXT_MAIN_IMAGE)
        {
          // monochrome
          Set_loading_layer(context, 0);
          Set_pixel(context, x, y, ((b & 1) * (is_dhgr ? 15 : 3)) + palette);
          Set_loading_layer(context, 1);
        }
        // color
        color = (color << 1) | (b & 1);
        if (is_dhgr)
        {
          if ((x & 3) == 0)
            previous_palette = palette; // what is important is the value when the 1st bit was read...
          /// emulate "chat mauve" DHGR mixed mode.
          /// see http://boutillon.free.fr/Underground/Anim_Et_Graph/Extasie_Chat_Mauve_Reloaded/Extasie_Chat_Mauve_Reloaded.html
          if (previous_palette) // BW
            Set_pixel(context, x, y, ((b & 1) * 15) + palette);
          else if ((x & 3) == 3)
          {
            Set_pixel(context, x - 3, y, (color & 15) + palette);
            Set_pixel(context, x - 2, y, (color & 15) + palette);
            Set_pixel(context, x - 1, y, (color & 15) + palette);
            Set_pixel(context, x, y, (color & 15) + palette);
          }
        }
        else
        {
          /// HGR emulation following the behaviour of a "Le Chat Mauve"
          /// RVB adapter for the Apple //c.
          /// Within the bit stream, the color of the middle pixel is :<br>
          /// <tt>
          /// 111 \          <br>
          /// 110  }- white  <br>
          /// 011 /          <br>
          /// 010 \ _ color  <br>
          /// 101 /          <br>
          /// 000 \          <br>
          /// 001  }- black  <br>
          /// 100 /          <br>
          /// </tt>
          /// Color depends on the selected palette for the current byte
          /// and the position of the pixel (odd or even).
          if ((color & 3) == 3) // 11 => white
          {
            Set_pixel(context, x - 1, y, 3 + previous_palette);
            Set_pixel(context, x, y, 3 + palette);
          }
          else if ((color & 1) == 0) // 0 => black
            Set_pixel(context, x, y, palette);
          else // 01 => color
          {
            if ((color & 7) == 5) // 101
              Set_pixel(context, x - 1, y, 2 - (x & 1) + previous_palette);
            Set_pixel(context, x, y, 2 - (x & 1) + palette);
          }
          previous_palette = palette;
        }
        b >>= 1;
        x++;
      }
    }
  }
  // show hidden data in HOLES
  for (y = 0; y < 64; y++)
  for (bank = 0; bank < 1; bank++)
  {
    byte b = 0;
    for (x = 0; x < 8; x++)
      b |= vram[bank][x + (y << 7) + 120];
    if (b != 0)
      GFX2_LogHexDump(GFX2_DEBUG, bank ? "AUX " : "MAIN", vram[bank], (y << 7) + 120, 8);
  }
  free(vram[0]);
  free(vram[1]);
  File_error = 0;

  Set_image_mode(context, is_dhgr ? IMAGE_MODE_DHGR : IMAGE_MODE_HGR);
}

/**
 * Save HGR (280x192) or DHGR (560x192) Apple II pictures
 *
 * The data saved is the "monochrome" data from layer 1
 */
void Save_HGR(T_IO_Context * context)
{
  FILE * file;
  byte * vram[2];
  int bank;
  int x, y;
  int is_dhgr = 0;

  File_error = 1;
  if (context->Height != 192 || (context->Width != 280 && context->Width != 560))
  {
    Warning_message("Picture must be 280x192 (HGR) or 560x192 (DHGR)");
    return;
  }
  if (context->Width == 560)
    is_dhgr = 1;

  file = Open_file_write(context);
  if (file == NULL)
    return;
  vram[0] = calloc(8192, 1);
  if (vram[0] == NULL)
  {
    fclose(file);
    return;
  }
  if (is_dhgr)
  {
    vram[1] = calloc(8192, 1);
    if (vram[1] == NULL)
    {
      free(vram[0]);
      fclose(file);
      return;
    }
  }
  else
    vram[1] = NULL;

  Set_saving_layer(context, 0); // "monochrome" layer
  for (y = 0; y < 192; y++)
  {
    int i, column = 0;
    int offset = ((y & 7) << 10) + ((y & 070) << 4) + ((y >> 6) * 40);
    x = 0;
    bank = 0;
    while (x < context->Width)
    {
      byte b;
      if (is_dhgr)
        b = (Get_pixel(context, x, y) & 16) ? 0 : 0x80;
      else
        b = (Get_pixel(context, x, y) & 4) ? 0x80 : 0;
      for (i = 0; i < 7; i++)
      {
        b = b | ((Get_pixel(context, x++, y) & 1) << i);
      }
      vram[bank][offset + column] = b;
      if (is_dhgr)
      {
        if (++bank > 1)
        {
          bank = 0;
          column++;
        }
      }
      else
        column++;
    }
  }

  if (Write_bytes(file, vram[0], 8192))
  {
    if (is_dhgr)
    {
      if (Write_bytes(file, vram[1], 8192))
        File_error = 0;
    }
    else
      File_error = 0;
  }

  free(vram[0]);
  free(vram[1]);
  fclose(file);
}


///////////////////////////// HP-48 Grob Files ////////////////////////////

/**
 * HP48 addresses are 20bits (5 nibbles)
 * offset is in nibble (half byte)
 */
static dword Read_HP48Address(const byte * buffer, int offset)
{
  dword data = 0;
  int i = 4;
  do
  {
    byte nibble;
    nibble = buffer[(offset + i) >> 1];
    if ((offset + i) & 1)
      nibble >>= 4;
    nibble &= 15;
    data = (data << 4) | nibble;
  }
  while (i-- > 0);
  return data;
}

/**
 * Test for a HP-48 Grob file
 */
void Test_GRB(T_IO_Context * context, FILE * file)
{
  byte buffer[18];
  unsigned long file_size;
  dword prologue, size, width, height;

  (void)context;
  File_error = 1;
  file_size = File_length_file(file);
  if (!Read_bytes(file, buffer, 18))
    return;
  if(memcmp(buffer, "HPHP48-R", 8) != 0)
    return;
  prologue = Read_HP48Address(buffer+8, 0);
  size = Read_HP48Address(buffer+8, 5);
  GFX2_Log(GFX2_DEBUG, "HP48 File detected. %lu bytes prologue %05x %u nibbles\n",
           file_size, prologue, size);
  if (prologue != 0x02b1e)
    return;
  height = Read_HP48Address(buffer+8, 10);
  width = Read_HP48Address(buffer+8, 15);
  GFX2_Log(GFX2_DEBUG, " Grob dimensions : %ux%u\n", width, height);
  if ((file_size - 8) < ((size + 5) / 2))
    return;
  if (file_size < (18 + ((width + 7) >> 3) * height))
    return;
  File_error = 0;
}

void Load_GRB(T_IO_Context * context)
{
  byte buffer[18];
  byte * bitplane[4];
  unsigned long file_size;
  dword prologue, size, width, height;
  byte bp, bpp;
  FILE * file;
  unsigned x, y;

  File_error = 1;
  file = Open_file_read(context);
  if (file == NULL)
    return;
  file_size = File_length_file(file);
  if (!Read_bytes(file, buffer, 18))
  {
    fclose(file);
    return;
  }
  prologue = Read_HP48Address(buffer+8, 0);
  size = Read_HP48Address(buffer+8, 5);
  height = Read_HP48Address(buffer+8, 10);
  width = Read_HP48Address(buffer+8, 15);
  if (height >= 256)
    bpp = 4;
  else if (height >= 192)
    bpp = 3;
  else if (height >= 128)
    bpp = 2;
  else
    bpp = 1;

  GFX2_Log(GFX2_DEBUG, "HP48: %05X size=%u %ux%u\n", prologue, size, width, height);

  File_error = 0;
  Pre_load(context, width, height/bpp, file_size, FORMAT_GRB, PIXEL_SIMPLE, bpp);
  if (File_error == 0)
  {
    dword bytes_per_plane = ((width + 7) >> 3) * (height/bpp);
    dword offset = 0;

    if (Config.Clear_palette)
      memset(context->Palette, 0, sizeof(T_Palette));
    for (x = 0; x < ((unsigned)1 << bpp); x++)
    {
      context->Palette[x].R = context->Palette[x].G = (x * 255) / ((1 << bpp) - 1);
      context->Palette[x].B = 127;
    }

    // Load all bit planes
    for (bp = 0; bp < bpp; bp++)
    {
      bitplane[bp] = GFX2_malloc(bytes_per_plane);
      if (bitplane[bp])
      {
        if (!Read_bytes(file, bitplane[bp], bytes_per_plane))
          File_error = 1;
      }
    }
    // set pixels
    for (y = 0; y < (height/bpp) && File_error == 0; y++)
    {
      for (x = 0; x < width; x++)
      {
        byte b = 0;
        for (bp = 0; bp < bpp; bp++)
          b |= ((bitplane[bp][offset] >> (x & 7)) & 1) << bp;
        // invert because 1 is a black pixel on HP-48 LCD display
        Set_pixel(context, x, y, b ^ ((1 << bpp) - 1));
        if ((x & 7) == 7)
          offset++;
      }
      if ((x & 7) != 7)
        offset++;
    }
    // Free bit planes
    for (bp = 0; bp < bpp; bp++)
      free(bitplane[bp]);
  }
  fclose(file);
}
