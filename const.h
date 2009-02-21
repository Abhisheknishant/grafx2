/*  Grafx2 - The Ultimate 256-color bitmap paint program

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
    along with Grafx2; if not, see <http://www.gnu.org/licenses/> or
    write to the Free Software Foundation, Inc.,
    59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#ifndef _CONST_H_
#define _CONST_H_



// D�claration des constantes ////////////////////////////////////////////////

#define M_2PI 6.28318530717958647692528676656

#define POURCENTAGE_VERSION     "98.0%" // Libell� du pourcentage de la version �
#define VERSION1                  2     // |
#define VERSION2                  0     // |_ Num�ro de version d�coup� en
#define BETA1                     98    // |  plusieurs parties => 2.0 �95.5%
#define BETA2                     0     // |  (utilis� pour le fichier de config)
#define ALPHA_BETA                "�"   // Type de la version "�" ou "�"
#define MAX_MODES_VIDEO           100   // Nombre de modes vid�o maxi
#define NB_BOUTONS                38    // Nombre de boutons � g�rer
#define NB_TOUCHES                134   // Nombre de combinaisons de touches
#define NB_TOUCHES_SPECIALES      72    // Nombre de touches sp�ciales
#define NB_OPERATIONS             33    // Nombre d'op�rations g�r�es par le moteur
#define NB_FACTEURS_DE_ZOOM       12    // Nombre de facteurs de zoom
#define LARGEUR_MENU              254   // Largeur du menu en taille r�elle
#define HAUTEUR_MENU              44    // Hauteur du menu en taille r�elle
#define NB_SPRITES_CURSEUR        8     // Nombre de sprites de curseur
#define LARGEUR_SPRITE_CURSEUR    15    // Largeur d'un sprite de curseur en pixels
#define HAUTEUR_SPRITE_CURSEUR    15    // Hauteur d'un sprite de curseur en pixels
#define NB_SPRITES_EFFETS         9     // Nombre de sprites d'effets
#define NB_SPRITES_MENU           18    // Nombre de sprites de menu
#define LARGEUR_SPRITE_MENU       14    // Largeur d'un sprite de menu en pixels
#define HAUTEUR_SPRITE_MENU       14    // Hauteur d'un sprite de menu en pixels
#define LARGEUR_PINCEAU           16    // Largeur d'un sprite de pinceau pr�d�fini
#define HAUTEUR_PINCEAU           16    // Hauteur d'un sprite de pinceau pr�d�fini
#define TAILLE_MAXI_PINCEAU       127   // Taille maxi des pinceaux
#define NB_SPRITES_DRIVES         7     // Nombre de sprites de drives
#define LARGEUR_SPRITE_DRIVE      8     // Largeur d'un sprite de drive en pixels
#define HAUTEUR_SPRITE_DRIVE      8     // Hauteur d'un sprite de drive en pixels
#define NB_SPRITES_PINCEAU        48    // Nombre de sprites de pinceau
#define NB_TRAMES_PREDEFINIES     12    // Nombre de trames pr�d�finies
#define TAILLE_PILE_OPERATIONS    16    // Nombre maximum d'�l�ments utilis�s par les op�rations
#define TAILLE_MAXI_PATH          37    // Taille maximum affichable du r�pertoire courant dans les fen�tres du fileselect
#define TAILLE_COMMENTAIRE        32    // Taille maxi des commentaires pour le PKM
#define NB_PAGES_UNDO_MAX         99    // Nombre maximal de pages d'undo
#define FACTEUR_DE_ZOOM_PAR_DEFAUT 4    // Facteur de zoom initial
#define TAILLE_CHEMIN_FICHIER     260   // Le nombre de caract�res maxi pour un nom de fichier avec chemin complet

// Caract�res pr�sents dans les deux fontes
#define CARACTERE_TRIANGLE_DROIT  16
#define CARACTERE_TRIANGLE_GAUCHE 17
#define CARACTERE_SUSPENSION      '�'

// On impose � l'allocation dynamique des pages de backups de conserver un
// minimum de 256 Ko pour que le reste du programme puisse continuer �
// fonctionner.
#define QUANTITE_MINIMALE_DE_MEMOIRE_A_CONSERVER (256*1024)

#define A_GAUCHE                  1     // Indique une direction (ou click) � gauche
#define A_DROITE                  2     // Indique une direction (ou click) � droite

#define LARGEUR_BARRE_SPLIT       6     // [ ۱�� ]
#define PROPORTION_SPLIT          0.3   // proportion de la zone non-zoom�e en largeur par rapport � l'�cran
#define NB_PIXELS_ZOOMES_MIN      4     // Nombre minimal de pixels zoom�s en largeur (Note: En dessous de 4, on ne peut plus scroller!)

#if defined(__MORPHOS__) || defined(__amigaos4__) || defined(__amigaos__)
   #define PARENT_DIR "/"
#else
   #define PARENT_DIR ".."
#endif

  // Les diff�rents formats de fichiers:

#ifndef __no_pnglib__
#define NB_FORMATS_CONNUS         13    // Nombre de formats connus (devrait �tre la valeur maximale de NB_FORMATS_LOAD et NB_FORMATS_SAVE, mais plus g�n�ralement: Card({NB_FORMATS_LOAD} UNION {NB_FORMATS_SAVE}))
#define NB_FORMATS_LOAD           13    // Nombre de formats que l'on sait charger
#define NB_FORMATS_SAVE           13    // Nombre de formats que l'on sait sauver
#else
// Without pnglib
#define NB_FORMATS_CONNUS         12    // Nombre de formats connus (devrait �tre la valeur maximale de NB_FORMATS_LOAD et NB_FORMATS_SAVE, mais plus g�n�ralement: Card({NB_FORMATS_LOAD} UNION {NB_FORMATS_SAVE}))
#define NB_FORMATS_LOAD           12    // Nombre de formats que l'on sait charger
#define NB_FORMATS_SAVE           12    // Nombre de formats que l'on sait sauver
#endif

enum FORMATS_RECONNUS
{
  FORMAT_PKM=1,      // |
  FORMAT_LBM,        // |
  FORMAT_GIF,        // |    Il faudra penser � r�ordonner
  FORMAT_BMP,        // |  les donn�es sur les formats dans
  FORMAT_PCX,        // |  GLOBAL.H si on modifie ces cons-
  FORMAT_IMG,        // |_ tantes.
  FORMAT_SCx,        // |
  FORMAT_PI1,        // |    Elles repr�sentent l'indice o�
  FORMAT_PC1,        // |  l'on doit aller piocher ces
  FORMAT_CEL,        // |  donn�es.
  FORMAT_KCF,        // |
  FORMAT_PAL,        // |
  FORMAT_PNG         // |
};

#define FORMAT_PAR_DEFAUT    FORMAT_GIF // Format par d�faut (ah bon? oh!)

  // Les diff�rentes erreurs:

enum CODES_D_ERREURS
{
  // 0 = Flash rouge de l'�cran, erreur non critique
  ERREUR_GUI_ABSENT=1,          // Le fichier gfx2gui.gif est absent
  ERREUR_GUI_CORROMPU,          // Mauvais fichier gfx2gui.gif
  ERREUR_INI_ABSENT,            // Le fichier gfx2def.ini est absent
  ERREUR_CFG_ABSENT,            // Le fichier GFX2.CFG est absent
  ERREUR_CFG_CORROMPU,          // Mauvais fichier GFX2.CFG
  ERREUR_CFG_ANCIEN,            // Ancienne version du fichier GFX2.CFG
  ERREUR_MEMOIRE,               // Plus de m�moire
  ERREUR_LIGNE_COMMANDE,        // Erreur sur la ligne de commande
  ERREUR_DRIVER_SOURIS,         // Pas de driver souris install�
  ERREUR_MODE_INTERDIT,         // Mode demand� sur la ligne de commande interdit (coch� en noir)
  ERREUR_SAUVEGARDE_CFG,        // Erreur en �criture pour GFX2.CFG
  ERREUR_REPERTOIRE_DISPARU,    // Le r�pertoire de lancement n'existe plus
  ERREUR_INI_CORROMPU,          // Le fichier GFX2.INI est corrompu
  ERREUR_SAUVEGARDE_INI,        // Le fichier GFX2.INI n'est pas inscriptible
  ERREUR_SORRY_SORRY_SORRY      // On le refera plus, promis (erreur d'allocation de page qui ne devrait JAMAIS se produire)
};

  // Les diff�rents types de pixels

enum PIXEL_RATIO
{
    PIXEL_SIMPLE,
    PIXEL_WIDE,
    PIXEL_TALL,
    PIXEL_DOUBLE
};

  // Les diff�rentes cat�gories de bouton:

enum FAMILLES_DE_BOUTONS
{
  FAMILLE_OUTIL=1,          // Outils de dessin (exemple : Freehand draw)
  FAMILLE_INTERRUPTION,     // Op�ration �ph�m�re (exemple : changement de brosse) > Interruption de l'op�ration courante pour faire autre chose, puis on revient
  FAMILLE_INSTANTANE,       // Pif paf (exemple : changement de couleur) > �a sera fini d�s qu'on lache le bouton, pas d'utilisation de la pile d'op�rations
  FAMILLE_TOOLBAR,          // Cache/Montre la barre d'outils
  FAMILLE_EFFETS            // Effets
};

  // Les diff�rentes formes de bouton:

enum FORMES_DE_BOUTONS
{
  FORME_BOUTON_SANS_CADRE,            // Ex: la palette
  FORME_BOUTON_RECTANGLE,             // Ex: la plupart
  FORME_BOUTON_TRIANGLE_HAUT_GAUCHE,  // Ex: Rectangles vides
  FORME_BOUTON_TRIANGLE_BAS_DROITE    // Ex: Rectangles pleins
};

  // Les diff�rentes formes de curseur:

enum FORMES_DE_CURSEUR
{
  FORME_CURSEUR_FLECHE,
  FORME_CURSEUR_CIBLE,              // Utilise le pinceau
  FORME_CURSEUR_CIBLE_PIPETTE,      // Utilise le pinceau
  FORME_CURSEUR_SABLIER,
  FORME_CURSEUR_MULTIDIRECTIONNEL,
  FORME_CURSEUR_HORIZONTAL,
  FORME_CURSEUR_CIBLE_FINE,         // Utilise le pinceau
  FORME_CURSEUR_CIBLE_PIPETTE_FINE, // Utilise le pinceau
  FORME_CURSEUR_CIBLE_XOR,
  FORME_CURSEUR_RECTANGLE_XOR,
  FORME_CURSEUR_ROTATE_XOR
};

  // Les diff�rentes formes de pinceaux (les types de pinceaux doivent �tre au d�but)

enum FORMES_DE_PINCEAUX
{
  FORME_PINCEAU_ROND,
  FORME_PINCEAU_CARRE,
  FORME_PINCEAU_BARRE_HORIZONTALE,
  FORME_PINCEAU_BARRE_VERTICALE,
  FORME_PINCEAU_SLASH,
  FORME_PINCEAU_ANTISLASH,
  FORME_PINCEAU_ALEATOIRE,
  FORME_PINCEAU_X,
  FORME_PINCEAU_PLUS,
  FORME_PINCEAU_LOSANGE,
  FORME_PINCEAU_ROND_TRAME,
  FORME_PINCEAU_CARRE_TRAME,
  FORME_PINCEAU_DIVERS,// Ce doit �tre le dernier des types de pinceaux, comme �a il indique le nombre de types de pinceaux (-1)
  FORME_PINCEAU_POINT, // Utilis� pour r�duire de pinceau � 1 point dans certaines op�rations
  FORME_PINCEAU_BROSSE_COULEUR,
  FORME_PINCEAU_BROSSE_MONOCHROME
};

  // Les diff�rents �tats de bouton:

#define BOUTON_RELACHE 0
#define BOUTON_ENFONCE 1

  // Les diff�rents modes de Shade

enum MODES_DE_SHADE
{
  MODE_SHADE_NORMAL,
  MODE_SHADE_BOUCLE,
  MODE_SHADE_NOSAT
};

  // Les diff�rents chunks du fichier .CFG

enum CHUNKS_CFG
{
  CHUNK_TOUCHES            = 0,
  CHUNK_MODES_VIDEO        = 1,
  CHUNK_SHADE              = 2,
  CHUNK_MASQUE             = 3,
  CHUNK_STENCIL            = 4,
  CHUNK_DEGRADES           = 5,
  CHUNK_SMOOTH             = 6,
  CHUNK_EXCLUDE_COLORS     = 7,
  CHUNK_QUICK_SHADE        = 8,
  CHUNK_GRILLE             = 9,
  CHUNK_MAX 
};

  // Les diff�rents types de lecteurs:

enum TYPES_DE_LECTEURS
{
  LECTEUR_FLOPPY_3_5,   // 0: Diskette 3"�
  LECTEUR_FLOPPY_5_25,  // 1: Diskette 5"�
  LECTEUR_HDD,          // 2: HDD
  LECTEUR_CDROM,        // 3: CD-ROM
  LECTEUR_NETWORK       // 4: Logique (r�seau?)
};

  // Les diff�rents boutons:

enum NUMEROS_DE_BOUTONS
{
  BOUTON_PINCEAUX,
  BOUTON_AJUSTER,
  BOUTON_DESSIN,
  BOUTON_COURBES,
  BOUTON_LIGNES,
  BOUTON_SPRAY,
  BOUTON_FLOODFILL,
  BOUTON_POLYGONES,
  BOUTON_POLYFILL,
  BOUTON_RECTANGLES,
  BOUTON_FILLRECT,
  BOUTON_CERCLES,
  BOUTON_FILLCERC,
  BOUTON_GRADRECT,
  BOUTON_GRADMENU,
  BOUTON_SPHERES,
  BOUTON_BROSSE,
  BOUTON_POLYBROSSE,
  BOUTON_EFFETS_BROSSE,
  BOUTON_EFFETS,
  BOUTON_TEXTE,
  BOUTON_LOUPE,
  BOUTON_PIPETTE,
  BOUTON_RESOL,
  BOUTON_PAGE,
  BOUTON_SAUVER,
  BOUTON_CHARGER,
  BOUTON_PARAMETRES,
  BOUTON_CLEAR,
  BOUTON_AIDE,
  BOUTON_UNDO,
  BOUTON_KILL,
  BOUTON_QUIT,
  BOUTON_PALETTE,
  BOUTON_PAL_LEFT,
  BOUTON_PAL_RIGHT,
  BOUTON_CHOIX_COL,
  BOUTON_CACHER
};

  // Les actions des touches sp�ciales

enum ACTIONS_SPECIALES
{
  SPECIAL_MOUSE_UP,                
  SPECIAL_MOUSE_DOWN,              
  SPECIAL_MOUSE_LEFT,              
  SPECIAL_MOUSE_RIGHT,             
  SPECIAL_CLICK_LEFT,               
  SPECIAL_CLICK_RIGHT,             
  SPECIAL_NEXT_FORECOLOR,         
  SPECIAL_PREVIOUS_FORECOLOR,      
  SPECIAL_NEXT_BACKCOLOR,          
  SPECIAL_PREVIOUS_BACKCOLOR,      
  SPECIAL_RETRECIR_PINCEAU,         
  SPECIAL_GROSSIR_PINCEAU,          
  SPECIAL_NEXT_USER_FORECOLOR,     
  SPECIAL_PREVIOUS_USER_FORECOLOR, 
  SPECIAL_NEXT_USER_BACKCOLOR,     
  SPECIAL_PREVIOUS_USER_BACKCOLOR, 
  SPECIAL_SCROLL_UP,
  SPECIAL_SCROLL_DOWN,
  SPECIAL_SCROLL_LEFT,
  SPECIAL_SCROLL_RIGHT,
  SPECIAL_SCROLL_UP_FAST,
  SPECIAL_SCROLL_DOWN_FAST,
  SPECIAL_SCROLL_LEFT_FAST,
  SPECIAL_SCROLL_RIGHT_FAST,
  SPECIAL_SCROLL_UP_SLOW,
  SPECIAL_SCROLL_DOWN_SLOW,
  SPECIAL_SCROLL_LEFT_SLOW,
  SPECIAL_SCROLL_RIGHT_SLOW,
  SPECIAL_SHOW_HIDE_CURSOR,
  SPECIAL_PINCEAU_POINT,
  SPECIAL_DESSIN_CONTINU,
  SPECIAL_FLIP_X,
  SPECIAL_FLIP_Y,
  SPECIAL_ROTATE_90,
  SPECIAL_ROTATE_180,
  SPECIAL_STRETCH,
  SPECIAL_DISTORT,
  SPECIAL_OUTLINE,
  SPECIAL_NIBBLE,
  SPECIAL_GET_BRUSH_COLORS,
  SPECIAL_RECOLORIZE_BRUSH,
  SPECIAL_ROTATE_ANY_ANGLE,
  SPECIAL_LOAD_BRUSH,
  SPECIAL_SAVE_BRUSH,
  SPECIAL_INVERT_SIEVE,
  SPECIAL_ZOOM_IN,
  SPECIAL_ZOOM_OUT,
  SPECIAL_CENTER_ATTACHMENT,
  SPECIAL_TOP_LEFT_ATTACHMENT,
  SPECIAL_TOP_RIGHT_ATTACHMENT,
  SPECIAL_BOTTOM_LEFT_ATTACHMENT,
  SPECIAL_BOTTOM_RIGHT_ATTACHMENT,
  SPECIAL_EXCLUDE_COLORS_MENU,
  SPECIAL_SHADE_MODE,       // |
  SPECIAL_SHADE_MENU,       // |
  SPECIAL_QUICK_SHADE_MODE, // |
  SPECIAL_QUICK_SHADE_MENU, // |
  SPECIAL_STENCIL_MODE,     // |
  SPECIAL_STENCIL_MENU,     // |
  SPECIAL_MASK_MODE,        // |  Il faut que le premier effet soit
  SPECIAL_MASK_MENU,        // |  SPECIAL_SHADE_MODE, et que le
  SPECIAL_GRID_MODE,        // |  dernier soit SPECIAL_TILING_MENU,
  SPECIAL_GRID_MENU,        // |_ et que seuls des effets soient
  SPECIAL_SIEVE_MODE,       // |  d�finis entre ces deux l� car
  SPECIAL_SIEVE_MENU,       // |  des tests sur cet intervalle sont
  SPECIAL_COLORIZE_MODE,    // |  faits dans le moteur.
  SPECIAL_COLORIZE_MENU,    // |
  SPECIAL_SMOOTH_MODE,      // |
  SPECIAL_SMOOTH_MENU,      // |
  SPECIAL_SMEAR_MODE,       // |
  SPECIAL_TILING_MODE,      // |
  SPECIAL_TILING_MENU       // |
};

  // D�finition des op�rations:

enum OPERATIONS
{
  OPERATION_DESSIN_CONTINU,     // Dessin � la main continu
  OPERATION_DESSIN_DISCONTINU,  // Dessin � la main discontinu
  OPERATION_DESSIN_POINT,       // Dessin � la main point par point
  OPERATION_FILLED_CONTOUR,     // Contour rempli
  OPERATION_LIGNE,              // Lignes
  OPERATION_K_LIGNE,            // Lignes reli�es
  OPERATION_LIGNES_CENTREES,    // Lignes concentriques
  OPERATION_RECTANGLE_VIDE,     // Rectangle vide
  OPERATION_RECTANGLE_PLEIN,    // Rectangle plein
  OPERATION_CERCLE_VIDE,        // Cercle vide
  OPERATION_CERCLE_PLEIN,       // Cercle plein
  OPERATION_ELLIPSE_VIDE,       // Ellipse vide
  OPERATION_ELLIPSE_PLEINE,     // Ellipse pleine
  OPERATION_FILL,               // Fill
  OPERATION_REMPLACER,          // Remplacer couleur
  OPERATION_PRISE_BROSSE,       // Prise de brosse rectangulaire
  OPERATION_POLYBROSSE,         // Prise d'une brosse multiforme
  OPERATION_PIPETTE,            // R�cup�ration d'une couleur
  OPERATION_LOUPE,              // Positionnement de la fen�tre de loupe
  OPERATION_COURBE_3_POINTS,    // Courbe � 3 points
  OPERATION_COURBE_4_POINTS,    // Courbe � 4 points
  OPERATION_SPRAY,              // Spray
  OPERATION_POLYGONE,           // Polygone
  OPERATION_POLYFORM,           // Polyforme
  OPERATION_POLYFILL,           // Polyfill
  OPERATION_FILLED_POLYFORM,    // Polyforme rempli
  OPERATION_SCROLL,             // Scroll
  OPERATION_CERCLE_DEGRADE,     // Cercles d�grad�s
  OPERATION_ELLIPSE_DEGRADEE,   // Ellipses d�grad�es
  OPERATION_TOURNER_BROSSE,     // Faire tourner brosse
  OPERATION_ETIRER_BROSSE,      // Etirer brosse
  OPERATION_DEFORMER_BROSSE,    // Deformer brosse
  OPERATION_RECTANGLE_DEGRADE,  // Rectangle d�grad�

  OPERATION_AUCUNE
};



#endif
