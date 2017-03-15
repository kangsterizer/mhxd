/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball, Josh MacDonald
 * Copyright (C) 1997-1998 Jay Painter <jpaint@serv.net><jpaint@gimp.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __GTK_HLIST_H__
#define __GTK_HLIST_H__

#include <gdk/gdk.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkhscrollbar.h>
#include <gtk/gtkvscrollbar.h>
#include <gtk/gtkenums.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* hlist flags */
enum {
  GTK_HLIST_IN_DRAG             = 1 <<  0,
  GTK_HLIST_ROW_HEIGHT_SET      = 1 <<  1,
  GTK_HLIST_SHOW_TITLES         = 1 <<  2,
  GTK_HLIST_CHILD_HAS_FOCUS     = 1 <<  3,
  GTK_HLIST_ADD_MODE            = 1 <<  4,
  GTK_HLIST_AUTO_SORT           = 1 <<  5,
  GTK_HLIST_AUTO_RESIZE_BLOCKED = 1 <<  6,
  GTK_HLIST_REORDERABLE         = 1 <<  7,
  GTK_HLIST_USE_DRAG_ICONS      = 1 <<  8,
  GTK_HLIST_DRAW_DRAG_LINE      = 1 <<  9,
  GTK_HLIST_DRAW_DRAG_RECT      = 1 << 10
}; 

/* cell types */
typedef enum
{
  GTK_HELL_EMPTY,
  GTK_HELL_TEXT,
  GTK_HELL_PIXMAP,
  GTK_HELL_PIXTEXT,
  GTK_HELL_WIDGET
} GtkHellType;

typedef enum
{
  GTK_HLIST_DRAG_NONE,
  GTK_HLIST_DRAG_BEFORE,
  GTK_HLIST_DRAG_INTO,
  GTK_HLIST_DRAG_AFTER
} GtkHListDragPos;

#define GTK_TYPE_HLIST            (gtk_hlist_get_type ())
#define GTK_HLIST(obj)            (GTK_CHECK_CAST ((obj), GTK_TYPE_HLIST, GtkHList))
#define GTK_HLIST_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_HLIST, GtkHListClass))
#define GTK_IS_HLIST(obj)         (GTK_CHECK_TYPE ((obj), GTK_TYPE_HLIST))
#define GTK_IS_HLIST_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_HLIST))

#define GTK_HLIST_FLAGS(hlist)             (GTK_HLIST (hlist)->flags)
#define GTK_HLIST_SET_FLAG(hlist,flag)     (GTK_HLIST_FLAGS (hlist) |= (GTK_ ## flag))
#define GTK_HLIST_UNSET_FLAG(hlist,flag)   (GTK_HLIST_FLAGS (hlist) &= ~(GTK_ ## flag))

#define GTK_HLIST_IN_DRAG(hlist)           (GTK_HLIST_FLAGS (hlist) & GTK_HLIST_IN_DRAG)
#define GTK_HLIST_ROW_HEIGHT_SET(hlist)    (GTK_HLIST_FLAGS (hlist) & GTK_HLIST_ROW_HEIGHT_SET)
#define GTK_HLIST_SHOW_TITLES(hlist)       (GTK_HLIST_FLAGS (hlist) & GTK_HLIST_SHOW_TITLES)
#define GTK_HLIST_CHILD_HAS_FOCUS(hlist)   (GTK_HLIST_FLAGS (hlist) & GTK_HLIST_CHILD_HAS_FOCUS)
#define GTK_HLIST_ADD_MODE(hlist)          (GTK_HLIST_FLAGS (hlist) & GTK_HLIST_ADD_MODE)
#define GTK_HLIST_AUTO_SORT(hlist)         (GTK_HLIST_FLAGS (hlist) & GTK_HLIST_AUTO_SORT)
#define GTK_HLIST_AUTO_RESIZE_BLOCKED(hlist) (GTK_HLIST_FLAGS (hlist) & GTK_HLIST_AUTO_RESIZE_BLOCKED)
#define GTK_HLIST_REORDERABLE(hlist)       (GTK_HLIST_FLAGS (hlist) & GTK_HLIST_REORDERABLE)
#define GTK_HLIST_USE_DRAG_ICONS(hlist)    (GTK_HLIST_FLAGS (hlist) & GTK_HLIST_USE_DRAG_ICONS)
#define GTK_HLIST_DRAW_DRAG_LINE(hlist)    (GTK_HLIST_FLAGS (hlist) & GTK_HLIST_DRAW_DRAG_LINE)
#define GTK_HLIST_DRAW_DRAG_RECT(hlist)    (GTK_HLIST_FLAGS (hlist) & GTK_HLIST_DRAW_DRAG_RECT)

#define GTK_HLIST_ROW(_glist_) ((GtkHListRow *)((_glist_)->data))

/* pointer casting for cells */
#define GTK_HELL_TEXT(cell)     (((GtkHellText *) &(cell)))
#define GTK_HELL_PIXMAP(cell)   (((GtkHellPixmap *) &(cell)))
#define GTK_HELL_PIXTEXT(cell)  (((GtkHellPixText *) &(cell)))
#define GTK_HELL_WIDGET(cell)   (((GtkHellWidget *) &(cell)))

typedef struct _GtkHList GtkHList;
typedef struct _GtkHListClass GtkHListClass;
typedef struct _GtkHListColumn GtkHListColumn;
typedef struct _GtkHListRow GtkHListRow;

typedef struct _GtkHell GtkHell;
typedef struct _GtkHellText GtkHellText;
typedef struct _GtkHellPixmap GtkHellPixmap;
typedef struct _GtkHellPixText GtkHellPixText;
typedef struct _GtkHellWidget GtkHellWidget;

typedef gint (*GtkHListCompareFunc) (GtkHList     *hlist,
				     gconstpointer ptr1,
				     gconstpointer ptr2);

typedef struct _GtkHListHellInfo GtkHListHellInfo;
typedef struct _GtkHListDestInfo GtkHListDestInfo;

struct _GtkHListHellInfo
{
  gint row;
  gint column;
};

struct _GtkHListDestInfo
{
  GtkHListHellInfo cell;
  GtkHListDragPos  insert_pos;
};

struct _GtkHList
{
  GtkContainer container;
  
  guint16 flags;
  
  /* mem chunks */
  GMemChunk *row_mem_chunk;
  GMemChunk *cell_mem_chunk;

  guint freeze_count;
  
  /* allocation rectangle after the conatiner_border_width
   * and the width of the shadow border */
  GdkRectangle internal_allocation;
  
  /* rows */
  gint rows;
  gint row_center_offset;
  gint *row_height;
  GList *row_list;
  GList *row_list_end;
  
  /* columns */
  gint columns;
  GdkRectangle column_title_area;
  GdkWindow *title_window;
  
  /* dynamicly allocated array of column structures */
  GtkHListColumn *column;
  
  /*the scrolling window and it's height and width to
   * make things a little speedier */
  GdkWindow *hlist_window;
  gint hlist_window_width;
  gint hlist_window_height;
  
  /* offsets for scrolling */
  gint hoffset;
  gint voffset;
  
  /* border shadow style */
  GtkShadowType shadow_type;
  
  /* the list's selection mode (gtkenums.h) */
  GtkSelectionMode selection_mode;
  
  /* list of selected rows */
  GList *selection;
  GList *selection_end;
  
  GList *undo_selection;
  GList *undo_unselection;
  gint undo_anchor;
  
  /* mouse buttons */
  guint8 button_actions[5];

  guint8 drag_button;

  /* dnd */
  GtkHListHellInfo click_cell;

  /* scroll adjustments */
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;
  
  /* xor GC for the vertical drag line */
  GdkGC *xor_gc;
  
  /* gc for drawing unselected cells */
  GdkGC *fg_gc;
  GdkGC *bg_gc;

  GdkBitmap *stipple_bitmap;
  GdkGC *stipple_gc;
  gint want_stipple;

  /* cursor used to indicate dragging */
  GdkCursor *cursor_drag;
  
  /* the current x-pixel location of the xor-drag line */
  gint x_drag;
  
  /* focus handling */
  gint focus_row;
  
  /* dragging the selection */
  gint anchor;
  GtkStateType anchor_state;
  gint drag_pos;
  gint htimer;
  gint vtimer;
  
  GtkSortType sort_type;
  GtkHListCompareFunc compare;
  gint sort_column;
};

struct _GtkHListClass
{
  GtkContainerClass parent_class;
  
  void  (*set_scroll_adjustments) (GtkHList       *hlist,
				   GtkAdjustment  *hadjustment,
				   GtkAdjustment  *vadjustment);
  void   (*refresh)             (GtkHList       *hlist);
  void   (*select_row)          (GtkHList       *hlist,
				 gint            row,
				 gint            column,
				 GdkEvent       *event);
  void   (*unselect_row)        (GtkHList       *hlist,
				 gint            row,
				 gint            column,
				 GdkEvent       *event);
  void   (*row_move)            (GtkHList       *hlist,
				 gint            source_row,
				 gint            dest_row);
  void   (*click_column)        (GtkHList       *hlist,
				 gint            column);
  void   (*resize_column)       (GtkHList       *hlist,
				 gint            column,
                                 gint            width);
  void   (*toggle_focus_row)    (GtkHList       *hlist);
  void   (*select_all)          (GtkHList       *hlist);
  void   (*unselect_all)        (GtkHList       *hlist);
  void   (*undo_selection)      (GtkHList       *hlist);
  void   (*start_selection)     (GtkHList       *hlist);
  void   (*end_selection)       (GtkHList       *hlist);
  void   (*extend_selection)    (GtkHList       *hlist,
				 GtkScrollType   scroll_type,
				 gfloat          position,
				 gboolean        auto_start_selection);
  void   (*scroll_horizontal)   (GtkHList       *hlist,
				 GtkScrollType   scroll_type,
				 gfloat          position);
  void   (*scroll_vertical)     (GtkHList       *hlist,
				 GtkScrollType   scroll_type,
				 gfloat          position);
  void   (*toggle_add_mode)     (GtkHList       *hlist);
  void   (*abort_column_resize) (GtkHList       *hlist);
  void   (*resync_selection)    (GtkHList       *hlist,
				 GdkEvent       *event);
  GList* (*selection_find)      (GtkHList       *hlist,
				 gint            row_number,
				 GList          *row_list_element);
  void   (*draw_row)            (GtkHList       *hlist,
				 GdkRectangle   *area,
				 gint            row,
				 GtkHListRow    *hlist_row);
  void   (*draw_drag_highlight) (GtkHList        *hlist,
				 GtkHListRow     *target_row,
				 gint             target_row_number,
				 GtkHListDragPos  drag_pos);
  void   (*clear)               (GtkHList       *hlist);
  void   (*fake_unselect_all)   (GtkHList       *hlist,
				 gint            row);
  void   (*sort_list)           (GtkHList       *hlist);
  gint   (*insert_row)          (GtkHList       *hlist,
				 gint            row,
				 gchar          *text[]);
  void   (*remove_row)          (GtkHList       *hlist,
				 gint            row);
  void   (*set_cell_contents)   (GtkHList       *hlist,
				 GtkHListRow    *hlist_row,
				 gint            column,
				 GtkHellType     type,
				 const gchar    *text,
				 guint8          spacing,
				 GdkPixmap      *pixmap,
				 GdkBitmap      *mask);
  void   (*cell_size_request)   (GtkHList       *hlist,
				 GtkHListRow    *hlist_row,
				 gint            column,
				 GtkRequisition *requisition);

};

struct _GtkHListColumn
{
  gchar *title;
  GdkRectangle area;
  
  GtkWidget *button;
  GdkWindow *window;
  
  gint width;
  gint min_width;
  gint max_width;
  GtkJustification justification;
  
  guint visible        : 1;  
  guint width_set      : 1;
  guint resizeable     : 1;
  guint auto_resize    : 1;
  guint button_passive : 1;
};

struct _GtkHListRow
{
  GtkHell *cell;
  GtkStateType state;
  
  GdkColor foreground;
  GdkColor background;
  
  GtkStyle *style;

  gpointer data;
  GtkDestroyNotify destroy;
  
  guint fg_set     : 1;
  guint bg_set     : 1;
  guint selectable : 1;
  guint draw_me    : 1;
};

/* Hell Structures */
struct _GtkHellText
{
  GtkHellType type;
  
  gint16 vertical;
  gint16 horizontal;
  
  GtkStyle *style;

  gchar *text;
};

struct _GtkHellPixmap
{
  GtkHellType type;
  
  gint16 vertical;
  gint16 horizontal;
  
  GtkStyle *style;

  GdkPixmap *pixmap;
  GdkBitmap *mask;
};

struct _GtkHellPixText
{
  GtkHellType type;
  
  gint16 vertical;
  gint16 horizontal;
  
  GtkStyle *style;

  gchar *text;
  guint8 spacing;
  GdkPixmap *pixmap;
  GdkBitmap *mask;
};

struct _GtkHellWidget
{
  GtkHellType type;
  
  gint16 vertical;
  gint16 horizontal;
  
  GtkStyle *style;

  GtkWidget *widget;
};

struct _GtkHell
{
  GtkHellType type;
  
  gint16 vertical;
  gint16 horizontal;
  
  GtkStyle *style;

  union {
    gchar *text;
    
    struct {
      GdkPixmap *pixmap;
      GdkBitmap *mask;
    } pm;
    
    struct {
      gchar *text;
      guint8 spacing;
      GdkPixmap *pixmap;
      GdkBitmap *mask;
    } pt;
    
    GtkWidget *widget;
  } u;
};

GtkType gtk_hlist_get_type (void);

/* constructors useful for gtk-- wrappers */
void gtk_hlist_construct (GtkHList *hlist,
			  gint      columns,
			  gchar    *titles[]);

/* create a new GtkHList */
GtkWidget* gtk_hlist_new             (gint   columns);
GtkWidget* gtk_hlist_new_with_titles (gint   columns,
				      gchar *titles[]);

/* stipple */
void gtk_hlist_set_stipple (GtkHList *hlist);

/* set adjustments of hlist */
void gtk_hlist_set_hadjustment (GtkHList      *hlist,
				GtkAdjustment *adjustment);
void gtk_hlist_set_vadjustment (GtkHList      *hlist,
				GtkAdjustment *adjustment);

/* get adjustments of hlist */
GtkAdjustment* gtk_hlist_get_hadjustment (GtkHList *hlist);
GtkAdjustment* gtk_hlist_get_vadjustment (GtkHList *hlist);

/* set the border style of the hlist */
void gtk_hlist_set_shadow_type (GtkHList      *hlist,
				GtkShadowType  type);

/* set the hlist's selection mode */
void gtk_hlist_set_selection_mode (GtkHList         *hlist,
				   GtkSelectionMode  mode);

/* enable hlists reorder ability */
void gtk_hlist_set_reorderable (GtkHList *hlist,
				gboolean  reorderable);
void gtk_hlist_set_use_drag_icons (GtkHList *hlist,
				   gboolean  use_icons);
void gtk_hlist_set_button_actions (GtkHList *hlist,
				   guint     button,
				   guint8    button_actions);

/* freeze all visual updates of the list, and then thaw the list after
 * you have made a number of changes and the updates wil occure in a
 * more efficent mannor than if you made them on a unfrozen list
 */
void gtk_hlist_freeze (GtkHList *hlist);
void gtk_hlist_thaw   (GtkHList *hlist);

/* show and hide the column title buttons */
void gtk_hlist_column_titles_show (GtkHList *hlist);
void gtk_hlist_column_titles_hide (GtkHList *hlist);

/* set the column title to be a active title (responds to button presses, 
 * prelights, and grabs keyboard focus), or passive where it acts as just
 * a title
 */
void gtk_hlist_column_title_active   (GtkHList *hlist,
				      gint      column);
void gtk_hlist_column_title_passive  (GtkHList *hlist,
				      gint      column);
void gtk_hlist_column_titles_active  (GtkHList *hlist);
void gtk_hlist_column_titles_passive (GtkHList *hlist);

/* set the title in the column title button */
void gtk_hlist_set_column_title (GtkHList    *hlist,
				 gint         column,
				 const gchar *title);

/* returns the title of column. Returns NULL if title is not set */
gchar * gtk_hlist_get_column_title (GtkHList *hlist,
				    gint      column);

/* set a widget instead of a title for the column title button */
void gtk_hlist_set_column_widget (GtkHList  *hlist,
				  gint       column,
				  GtkWidget *widget);

/* returns the column widget */
GtkWidget * gtk_hlist_get_column_widget (GtkHList *hlist,
					 gint      column);

/* set the justification on a column */
void gtk_hlist_set_column_justification (GtkHList         *hlist,
					 gint              column,
					 GtkJustification  justification);

/* set visibility of a column */
void gtk_hlist_set_column_visibility (GtkHList *hlist,
				      gint      column,
				      gboolean  visible);

/* enable/disable column resize operations by mouse */
void gtk_hlist_set_column_resizeable (GtkHList *hlist,
				      gint      column,
				      gboolean  resizeable);

/* resize column automatically to its optimal width */
void gtk_hlist_set_column_auto_resize (GtkHList *hlist,
				       gint      column,
				       gboolean  auto_resize);

gint gtk_hlist_columns_autosize (GtkHList *hlist);

/* return the optimal column width, i.e. maximum of all cell widths */
gint gtk_hlist_optimal_column_width (GtkHList *hlist,
				     gint      column);

/* set the pixel width of a column; this is a necessary step in
 * creating a HList because otherwise the column width is chozen from
 * the width of the column title, which will never be right
 */
void gtk_hlist_set_column_width (GtkHList *hlist,
				 gint      column,
				 gint      width);

/* set column minimum/maximum width. min/max_width < 0 => no restriction */
void gtk_hlist_set_column_min_width (GtkHList *hlist,
				     gint      column,
				     gint      min_width);
void gtk_hlist_set_column_max_width (GtkHList *hlist,
				     gint      column,
				     gint      max_width);

/* change the height of the rows, the default (height=0) is
 * the hight of the current font.
 */
void gtk_hlist_set_row_height (GtkHList *hlist,
			       guint     height);

/* scroll the viewing area of the list to the given column and row;
 * row_align and col_align are between 0-1 representing the location the
 * row should appear on the screnn, 0.0 being top or left, 1.0 being
 * bottom or right; if row or column is -1 then then there is no change
 */
void gtk_hlist_moveto (GtkHList *hlist,
		       gint      row,
		       gint      column,
		       gfloat    row_align,
		       gfloat    col_align);

/* returns whether the row is visible */
GtkVisibility gtk_hlist_row_is_visible (GtkHList *hlist,
					gint      row);

/* returns the cell type */
GtkHellType gtk_hlist_get_cell_type (GtkHList *hlist,
				     gint      row,
				     gint      column);

/* sets a given cell's text, replacing it's current contents */
void gtk_hlist_set_text (GtkHList    *hlist,
			 gint         row,
			 gint         column,
			 const gchar *text);

/* for the "get" functions, any of the return pointer can be
 * NULL if you are not interested
 */
gint gtk_hlist_get_text (GtkHList  *hlist,
			 gint       row,
			 gint       column,
			 gchar    **text);

/* sets a given cell's pixmap, replacing it's current contents */
void gtk_hlist_set_pixmap (GtkHList  *hlist,
			   gint       row,
			   gint       column,
			   GdkPixmap *pixmap,
			   GdkBitmap *mask);

gint gtk_hlist_get_pixmap (GtkHList   *hlist,
			   gint        row,
			   gint        column,
			   GdkPixmap **pixmap,
			   GdkBitmap **mask);

/* sets a given cell's pixmap and text, replacing it's current contents */
void gtk_hlist_set_pixtext (GtkHList    *hlist,
			    gint         row,
			    gint         column,
			    const gchar *text,
			    guint8       spacing,
			    GdkPixmap   *pixmap,
			    GdkBitmap   *mask);

gint gtk_hlist_get_pixtext (GtkHList   *hlist,
			    gint        row,
			    gint        column,
			    gchar     **text,
			    guint8     *spacing,
			    GdkPixmap **pixmap,
			    GdkBitmap **mask);

/* sets the foreground color of a row, the color must already
 * be allocated
 */
void gtk_hlist_set_foreground (GtkHList *hlist,
			       gint      row,
			       GdkColor *color);

/* sets the background color of a row, the color must already
 * be allocated
 */
void gtk_hlist_set_background (GtkHList *hlist,
			       gint      row,
			       GdkColor *color);

/* set / get cell styles */
void gtk_hlist_set_cell_style (GtkHList *hlist,
			       gint      row,
			       gint      column,
			       GtkStyle *style);

GtkStyle *gtk_hlist_get_cell_style (GtkHList *hlist,
				    gint      row,
				    gint      column);

void gtk_hlist_set_row_style (GtkHList *hlist,
			      gint      row,
			      GtkStyle *style);

GtkStyle *gtk_hlist_get_row_style (GtkHList *hlist,
				   gint      row);

/* this sets a horizontal and vertical shift for drawing
 * the contents of a cell; it can be positive or negitive;
 * this is particulary useful for indenting items in a column
 */
void gtk_hlist_set_shift (GtkHList *hlist,
			  gint      row,
			  gint      column,
			  gint      vertical,
			  gint      horizontal);

/* set/get selectable flag of a single row */
void gtk_hlist_set_selectable (GtkHList *hlist,
			       gint      row,
			       gboolean  selectable);
gboolean gtk_hlist_get_selectable (GtkHList *hlist,
				   gint      row);

/* prepend/append returns the index of the row you just added,
 * making it easier to append and modify a row
 */
gint gtk_hlist_prepend (GtkHList    *hlist,
		        gchar       *text[]);
gint gtk_hlist_append  (GtkHList    *hlist,
			gchar       *text[]);

/* inserts a row at index row and returns the row where it was
 * actually inserted (may be different from "row" in auto_sort mode)
 */
gint gtk_hlist_insert (GtkHList    *hlist,
		       gint         row,
		       gchar       *text[]);

/* removes row at index row */
void gtk_hlist_remove (GtkHList *hlist,
		       gint      row);

/* sets a arbitrary data pointer for a given row */
void gtk_hlist_set_row_data (GtkHList *hlist,
			     gint      row,
			     gpointer  data);

/* sets a data pointer for a given row with destroy notification */
void gtk_hlist_set_row_data_full (GtkHList         *hlist,
			          gint              row,
			          gpointer          data,
				  GtkDestroyNotify  destroy);

/* returns the data set for a row */
gpointer gtk_hlist_get_row_data (GtkHList *hlist,
				 gint      row);

/* givin a data pointer, find the first (and hopefully only!)
 * row that points to that data, or -1 if none do
 */
gint gtk_hlist_find_row_from_data (GtkHList *hlist,
				   gpointer  data);

/* force selection of a row */
void gtk_hlist_select_row (GtkHList *hlist,
			   gint      row,
			   gint      column);

/* force unselection of a row */
void gtk_hlist_unselect_row (GtkHList *hlist,
			     gint      row,
			     gint      column);

/* undo the last select/unselect operation */
void gtk_hlist_undo_selection (GtkHList *hlist);

/* clear the entire list -- this is much faster than removing
 * each item with gtk_hlist_remove
 */
void gtk_hlist_clear (GtkHList *hlist);

/* return the row column corresponding to the x and y coordinates,
 * the returned values are only valid if the x and y coordinates
 * are respectively to a window == hlist->hlist_window
 */
gint gtk_hlist_get_selection_info (GtkHList *hlist,
			     	   gint      x,
			     	   gint      y,
			     	   gint     *row,
			     	   gint     *column);

/* in multiple or extended mode, select all rows */
void gtk_hlist_select_all (GtkHList *hlist);

/* in all modes except browse mode, deselect all rows */
void gtk_hlist_unselect_all (GtkHList *hlist);

/* swap the position of two rows */
void gtk_hlist_swap_rows (GtkHList *hlist,
			  gint      row1,
			  gint      row2);

/* move row from source_row position to dest_row position */
void gtk_hlist_row_move (GtkHList *hlist,
			 gint      source_row,
			 gint      dest_row);

/* sets a compare function different to the default */
void gtk_hlist_set_compare_func (GtkHList            *hlist,
				 GtkHListCompareFunc  cmp_func);

/* the column to sort by */
void gtk_hlist_set_sort_column (GtkHList *hlist,
				gint      column);

/* how to sort : ascending or descending */
void gtk_hlist_set_sort_type (GtkHList    *hlist,
			      GtkSortType  sort_type);

/* sort the list with the current compare function */
void gtk_hlist_sort (GtkHList *hlist);

/* Automatically sort upon insertion */
void gtk_hlist_set_auto_sort (GtkHList *hlist,
			      gboolean  auto_sort);


#ifdef __cplusplus
}
#endif				/* __cplusplus */

#endif				/* __GTK_HLIST_H__ */
