/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball, Josh MacDonald, 
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gtk_hlist.h"
#include <gtk/gtk.h>
/* #include <gdk/gdkx.h> */
#include <gdk/gdkkeysyms.h>

/* length of button_actions array */
#define MAX_BUTTON 5

/* the number rows memchunk expands at a time */
#define HLIST_OPTIMUM_SIZE 64

/* the width of the column resize windows */
#define DRAG_WIDTH  6

/* minimum allowed width of a column */
#define COLUMN_MIN_WIDTH 5

/* this defigns the base grid spacing */
/* was CELL_SPACING ? */
#define HELL_SPACING 0

/* added the horizontal space at the beginning and end of a row*/
#define COLUMN_INSET 0

/* used for auto-scrolling */
#define SCROLL_TIME  100

static gint
height_of_row (GtkHList *hlist, gint row)
{
	gint i;
	gint h;

	h = 0;
	for (i = 0; i < row; i++)
		h += hlist->row_height[i];

	return h;
}

/* gives the top pixel of the given row in context of
 * the hlist's voffset */
/* #define ROW_TOP_YPIXEL(hlist, row) (((hlist)->row_height * (row)) + */
#define ROW_TOP_YPIXEL(hlist, row) (  height_of_row(hlist, row) + \
				    (((row) + 1) * HELL_SPACING) + \
				    (hlist)->voffset)


static gint
row_of_height (GtkHList *hlist, guint height)
{
	guint y;
	gint i, row;

	y = 0;
	row = hlist->rows-1;;
	for (i = 0; i < hlist->rows; i++) {
		y += hlist->row_height[i];
		if (y > height) {
			row = i;
			break;
		}
	}

	return row;
}

/* returns the row index from a y pixel location in the 
 * context of the hlist's voffset */
/* #define ROW_FROM_YPIXEL(hlist, y)  (((y) - (hlist)->voffset) / 
				    ((hlist)->row_height + HELL_SPACING)) */
#define ROW_FROM_YPIXEL(hlist, y)  (row_of_height((hlist), ((y) - (hlist)->voffset)))

/* gives the left pixel of the given column in context of
 * the hlist's hoffset */
#define COLUMN_LEFT_XPIXEL(hlist, colnum)  ((hlist)->column[(colnum)].area.x + \
					    (hlist)->hoffset)

/* returns the column index from a x pixel location in the 
 * context of the hlist's hoffset */
static inline gint
COLUMN_FROM_XPIXEL (GtkHList * hlist,
		    gint x)
{
  gint i, cx;

  for (i = 0; i < hlist->columns; i++)
    if (hlist->column[i].visible)
      {
	cx = hlist->column[i].area.x + hlist->hoffset;

	if (x >= (cx - (COLUMN_INSET + HELL_SPACING)) &&
	    x <= (cx + hlist->column[i].area.width + COLUMN_INSET))
	  return i;
      }

  /* no match */
  return -1;
}

/* returns the top pixel of the given row in the context of
 * the list height */
#define ROW_TOP(hlist, row)        (height_of_row(row)+HELL_SPACING*row)

/* returns the left pixel of the given column in the context of
 * the list width */
#define COLUMN_LEFT(hlist, colnum) ((hlist)->column[(colnum)].area.x)

/* returns the total height of the list */
#define LIST_HEIGHT(hlist)         (height_of_row(hlist,hlist->rows-1)+HELL_SPACING*hlist->rows)


/* returns the total width of the list */
static inline gint
LIST_WIDTH (GtkHList * hlist) 
{
  gint last_column;

  for (last_column = hlist->columns - 1;
       last_column >= 0 && !hlist->column[last_column].visible; last_column--);

  if (last_column >= 0)
    return (hlist->column[last_column].area.x +
	    hlist->column[last_column].area.width +
	    COLUMN_INSET + HELL_SPACING);
  return 0;
}

#define GTK_HLIST_CLASS_FW(_widget_) GTK_HLIST_CLASS (((GtkObject*) (_widget_))->klass)

/* redraw the list if it's not frozen */
#define HLIST_UNFROZEN(hlist)     (((GtkHList*) (hlist))->freeze_count == 0)
#define	HLIST_REFRESH(hlist)	G_STMT_START { \
  if (HLIST_UNFROZEN (hlist)) \
    GTK_HLIST_CLASS_FW (hlist)->refresh ((GtkHList*) (hlist)); \
} G_STMT_END


/* Signals */
enum {
  SELECT_ROW,
  UNSELECT_ROW,
  ROW_MOVE,
  CLICK_COLUMN,
  RESIZE_COLUMN,
  TOGGLE_FOCUS_ROW,
  SELECT_ALL,
  UNSELECT_ALL,
  UNDO_SELECTION,
  START_SELECTION,
  END_SELECTION,
  TOGGLE_ADD_MODE,
  EXTEND_SELECTION,
  SCROLL_VERTICAL,
  SCROLL_HORIZONTAL,
  ABORT_COLUMN_RESIZE,
  LAST_SIGNAL
};

enum {
  SYNC_REMOVE,
  SYNC_INSERT
};

enum {
  ARG_0,
  ARG_N_COLUMNS,
  ARG_SHADOW_TYPE,
  ARG_SELECTION_MODE,
  ARG_ROW_HEIGHT,
  ARG_TITLES_ACTIVE,
  ARG_REORDERABLE,
  ARG_USE_DRAG_ICONS,
  ARG_SORT_TYPE
};

/* GtkHList Methods */
static void gtk_hlist_class_init (GtkHListClass *klass);
static void gtk_hlist_init       (GtkHList      *hlist);

/* GtkObject Methods */
static void gtk_hlist_destroy  (GtkObject *object);
static void gtk_hlist_finalize (GtkObject *object);
static void gtk_hlist_set_arg  (GtkObject *object,
				GtkArg    *arg,
				guint      arg_id);
static void gtk_hlist_get_arg  (GtkObject *object,
				GtkArg    *arg,
				guint      arg_id);

/* GtkWidget Methods */
static void gtk_hlist_set_scroll_adjustments (GtkHList      *hlist,
					      GtkAdjustment *hadjustment,
					      GtkAdjustment *vadjustment);
static void gtk_hlist_realize         (GtkWidget        *widget);
static void gtk_hlist_unrealize       (GtkWidget        *widget);
static void gtk_hlist_map             (GtkWidget        *widget);
static void gtk_hlist_unmap           (GtkWidget        *widget);
static void gtk_hlist_draw            (GtkWidget        *widget,
			               GdkRectangle     *area);
static gint gtk_hlist_expose          (GtkWidget        *widget,
			               GdkEventExpose   *event);
static gint gtk_hlist_key_press       (GtkWidget        *widget,
				       GdkEventKey      *event);
static gint gtk_hlist_button_press    (GtkWidget        *widget,
				       GdkEventButton   *event);
static gint gtk_hlist_button_release  (GtkWidget        *widget,
				       GdkEventButton   *event);
static gint gtk_hlist_motion          (GtkWidget        *widget, 
			               GdkEventMotion   *event);
static void gtk_hlist_size_request    (GtkWidget        *widget,
				       GtkRequisition   *requisition);
static void gtk_hlist_size_allocate   (GtkWidget        *widget,
				       GtkAllocation    *allocation);
static void gtk_hlist_draw_focus      (GtkWidget        *widget);
static gint gtk_hlist_focus_in        (GtkWidget        *widget,
				       GdkEventFocus    *event);
static gint gtk_hlist_focus_out       (GtkWidget        *widget,
				       GdkEventFocus    *event);
static gint gtk_hlist_focus           (GtkContainer     *container,
				       GtkDirectionType  direction);
static void gtk_hlist_style_set       (GtkWidget        *widget,
				       GtkStyle         *previous_style);
static void gtk_hlist_drag_begin      (GtkWidget        *widget,
				       GdkDragContext   *context);
static gint gtk_hlist_drag_motion     (GtkWidget        *widget,
				       GdkDragContext   *context,
				       gint              x,
				       gint              y,
				       guint             time);
static void gtk_hlist_drag_leave      (GtkWidget        *widget,
				       GdkDragContext   *context,
				       guint             time);
static void gtk_hlist_drag_end        (GtkWidget        *widget,
				       GdkDragContext   *context);
static gboolean gtk_hlist_drag_drop   (GtkWidget      *widget,
				       GdkDragContext *context,
				       gint            x,
				       gint            y,
				       guint           time);
static void gtk_hlist_drag_data_get   (GtkWidget        *widget,
				       GdkDragContext   *context,
				       GtkSelectionData *selection_data,
				       guint             info,
				       guint             time);
static void gtk_hlist_drag_data_received (GtkWidget        *widget,
					  GdkDragContext   *context,
					  gint              x,
					  gint              y,
					  GtkSelectionData *selection_data,
					  guint             info,
					  guint             time);

/* GtkContainer Methods */
static void gtk_hlist_set_focus_child (GtkContainer  *container,
				       GtkWidget     *child);
static void gtk_hlist_forall          (GtkContainer  *container,
			               gboolean       include_internals,
			               GtkCallback    callback,
			               gpointer       callback_data);

/* Selection */
static void toggle_row                (GtkHList      *hlist,
			               gint           row,
			               gint           column,
			               GdkEvent      *event);
static void real_select_row           (GtkHList      *hlist,
			               gint           row,
			               gint           column,
			               GdkEvent      *event);
static void real_unselect_row         (GtkHList      *hlist,
			               gint           row,
			               gint           column,
			               GdkEvent      *event);
static void update_extended_selection (GtkHList      *hlist,
				       gint           row);
static GList *selection_find          (GtkHList      *hlist,
			               gint           row_number,
			               GList         *row_list_element);
static void real_select_all           (GtkHList      *hlist);
static void real_unselect_all         (GtkHList      *hlist);
static void move_vertical             (GtkHList      *hlist,
			               gint           row,
			               gfloat         align);
static void move_horizontal           (GtkHList      *hlist,
			               gint           diff);
static void real_undo_selection       (GtkHList      *hlist);
static void fake_unselect_all         (GtkHList      *hlist,
			               gint           row);
static void fake_toggle_row           (GtkHList      *hlist,
			               gint           row);
static void resync_selection          (GtkHList      *hlist,
			               GdkEvent      *event);
static void sync_selection            (GtkHList      *hlist,
	                               gint           row,
                                       gint           mode);
static void set_anchor                (GtkHList      *hlist,
			               gboolean       add_mode,
			               gint           anchor,
			               gint           undo_anchor);
static void start_selection           (GtkHList      *hlist);
static void end_selection             (GtkHList      *hlist);
static void toggle_add_mode           (GtkHList      *hlist);
static void toggle_focus_row          (GtkHList      *hlist);
static void extend_selection          (GtkHList      *hlist,
			               GtkScrollType  scroll_type,
			               gfloat         position,
			               gboolean       auto_start_selection);
static gint get_selection_info        (GtkHList       *hlist,
				       gint            x,
				       gint            y,
				       gint           *row,
				       gint           *column);

/* Scrolling */
static void move_focus_row     (GtkHList      *hlist,
			        GtkScrollType  scroll_type,
			        gfloat         position);
static void scroll_horizontal  (GtkHList      *hlist,
			        GtkScrollType  scroll_type,
			        gfloat         position);
static void scroll_vertical    (GtkHList      *hlist,
			        GtkScrollType  scroll_type,
			        gfloat         position);
static void move_horizontal    (GtkHList      *hlist,
				gint           diff);
static void move_vertical      (GtkHList      *hlist,
				gint           row,
				gfloat         align);
static gint horizontal_timeout (GtkHList      *hlist);
static gint vertical_timeout   (GtkHList      *hlist);
static void remove_grab        (GtkHList      *hlist);


/* Resize Columns */
static void draw_xor_line             (GtkHList       *hlist);
static gint new_column_width          (GtkHList       *hlist,
			               gint            column,
			               gint           *x);
static void column_auto_resize        (GtkHList       *hlist,
				       GtkHListRow    *hlist_row,
				       gint            column,
				       gint            old_width);
static void real_resize_column        (GtkHList       *hlist,
				       gint            column,
				       gint            width);
static void abort_column_resize       (GtkHList       *hlist);
static void cell_size_request         (GtkHList       *hlist,
			               GtkHListRow    *hlist_row,
			               gint            column,
				       GtkRequisition *requisition);

/* Buttons */
static void column_button_create      (GtkHList       *hlist,
				       gint            column);
static void column_button_clicked     (GtkWidget      *widget,
				       gpointer        data);

/* Adjustments */
static void adjust_adjustments        (GtkHList       *hlist,
				       gboolean        block_resize);
static void check_exposures           (GtkHList       *hlist);
static void vadjustment_changed       (GtkAdjustment  *adjustment,
				       gpointer        data);
static void vadjustment_value_changed (GtkAdjustment  *adjustment,
				       gpointer        data);
static void hadjustment_changed       (GtkAdjustment  *adjustment,
				       gpointer        data);
static void hadjustment_value_changed (GtkAdjustment  *adjustment,
				       gpointer        data);

/* Drawing */
static void get_cell_style   (GtkHList      *hlist,
			      GtkHListRow   *hlist_row,
			      gint           state,
			      gint           column,
			      GtkStyle     **style,
			      GdkGC        **fg_gc,
			      GdkGC        **bg_gc);
static gint draw_cell_pixmap (GdkWindow     *window,
			      GdkRectangle  *clip_rectangle,
			      GdkGC         *fg_gc,
			      GdkGC         *fill_gc,
			      GdkPixmap     *pixmap,
			      GdkBitmap     *mask,
			      gint           x,
			      gint           y,
			      gint           width,
			      gint           height);
static void draw_row         (GtkHList      *hlist,
			      GdkRectangle  *area,
			      gint           row,
			      GtkHListRow   *hlist_row);
static void draw_rows        (GtkHList      *hlist,
			      GdkRectangle  *area);
static void hlist_refresh    (GtkHList      *hlist);
static void draw_drag_highlight (GtkHList        *hlist,
				 GtkHListRow     *dest_row,
				 gint             dest_row_number,
				 GtkHListDragPos  drag_pos);
     
/* Size Allocation / Requisition */
static void size_allocate_title_buttons (GtkHList *hlist);
static void size_allocate_columns       (GtkHList *hlist,
					 gboolean  block_resize);
static gint list_requisition_width      (GtkHList *hlist);

/* Memory Allocation/Distruction Routines */
static GtkHListColumn *columns_new (GtkHList      *hlist);
static void column_title_new       (GtkHList      *hlist,
			            gint           column,
			            const gchar   *title);
static void columns_delete         (GtkHList      *hlist);
static GtkHListRow *row_new        (GtkHList      *hlist);
static void row_delete             (GtkHList      *hlist,
			            GtkHListRow   *hlist_row);
static void set_cell_contents      (GtkHList      *hlist,
			            GtkHListRow   *hlist_row,
				    gint           column,
				    GtkHellType    type,
				    const gchar   *text,
				    guint8         spacing,
				    GdkPixmap     *pixmap,
				    GdkBitmap     *mask);
static gint real_insert_row        (GtkHList      *hlist,
				    gint           row,
				    gchar         *text[]);
static void real_remove_row        (GtkHList      *hlist,
				    gint           row);
static void real_clear             (GtkHList      *hlist);

/* Sorting */
static gint default_compare        (GtkHList      *hlist,
			            gconstpointer  row1,
			            gconstpointer  row2);
static void real_sort_list         (GtkHList      *hlist);
static GList *gtk_hlist_merge      (GtkHList      *hlist,
				    GList         *a,
				    GList         *b);
static GList *gtk_hlist_mergesort  (GtkHList      *hlist,
				    GList         *list,
				    gint           num);
/* Misc */
static gboolean title_focus           (GtkHList  *hlist,
			               gint       dir);
static void real_row_move             (GtkHList  *hlist,
			               gint       source_row,
			               gint       dest_row);
static gint column_title_passive_func (GtkWidget *widget, 
				       GdkEvent  *event,
				       gpointer   data);
static void drag_dest_cell            (GtkHList         *hlist,
				       gint              x,
				       gint              y,
				       GtkHListDestInfo *dest_info);



static GtkContainerClass *parent_class = NULL;
static guint hlist_signals[LAST_SIGNAL] = {0};

static GtkTargetEntry hlist_target_table = { "gtk-hlist-drag-reorder", 0, 0};

GtkType
gtk_hlist_get_type (void)
{
  static GtkType hlist_type = 0;

  if (!hlist_type)
    {
      static const GtkTypeInfo hlist_info =
      {
	"GtkHList",
	sizeof (GtkHList),
	sizeof (GtkHListClass),
	(GtkClassInitFunc) gtk_hlist_class_init,
	(GtkObjectInitFunc) gtk_hlist_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      hlist_type = gtk_type_unique (GTK_TYPE_CONTAINER, &hlist_info);
    }

  return hlist_type;
}

static void
gtk_hlist_class_init (GtkHListClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;
  GtkBindingSet *binding_set;

  object_class = (GtkObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;
  container_class = (GtkContainerClass *) klass;

  parent_class = gtk_type_class (GTK_TYPE_CONTAINER);

  gtk_object_add_arg_type ("GtkHList::n_columns",
			   GTK_TYPE_UINT,
			   GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT_ONLY,
			   ARG_N_COLUMNS);
  gtk_object_add_arg_type ("GtkHList::shadow_type",
			   GTK_TYPE_SHADOW_TYPE,
			   GTK_ARG_READWRITE,
			   ARG_SHADOW_TYPE);
  gtk_object_add_arg_type ("GtkHList::selection_mode",
			   GTK_TYPE_SELECTION_MODE,
			   GTK_ARG_READWRITE,
			   ARG_SELECTION_MODE);
  gtk_object_add_arg_type ("GtkHList::row_height",
			   GTK_TYPE_UINT,
			   GTK_ARG_READWRITE,
			   ARG_ROW_HEIGHT);
  gtk_object_add_arg_type ("GtkHList::reorderable",
			   GTK_TYPE_BOOL,
			   GTK_ARG_READWRITE,
			   ARG_REORDERABLE);
  gtk_object_add_arg_type ("GtkHList::titles_active",
			   GTK_TYPE_BOOL,
			   GTK_ARG_READWRITE,
			   ARG_TITLES_ACTIVE);
  gtk_object_add_arg_type ("GtkHList::use_drag_icons",
			   GTK_TYPE_BOOL,
			   GTK_ARG_READWRITE,
			   ARG_USE_DRAG_ICONS);
  gtk_object_add_arg_type ("GtkHList::sort_type",
			   GTK_TYPE_SORT_TYPE,
			   GTK_ARG_READWRITE,
			   ARG_SORT_TYPE);  
  object_class->set_arg = gtk_hlist_set_arg;
  object_class->get_arg = gtk_hlist_get_arg;
  object_class->destroy = gtk_hlist_destroy;
  object_class->finalize = gtk_hlist_finalize;


  widget_class->set_scroll_adjustments_signal =
    gtk_signal_new ("set_scroll_adjustments",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkHListClass, set_scroll_adjustments),
		    gtk_marshal_NONE__POINTER_POINTER,
		    GTK_TYPE_NONE, 2, GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);

  hlist_signals[SELECT_ROW] =
    gtk_signal_new ("select_row",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkHListClass, select_row),
		    gtk_marshal_NONE__INT_INT_POINTER,
		    GTK_TYPE_NONE, 3,
		    GTK_TYPE_INT,
		    GTK_TYPE_INT,
		    GTK_TYPE_GDK_EVENT);
  hlist_signals[UNSELECT_ROW] =
    gtk_signal_new ("unselect_row",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkHListClass, unselect_row),
		    gtk_marshal_NONE__INT_INT_POINTER,
		    GTK_TYPE_NONE, 3, GTK_TYPE_INT,
		    GTK_TYPE_INT, GTK_TYPE_GDK_EVENT);
  hlist_signals[ROW_MOVE] =
    gtk_signal_new ("row_move",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkHListClass, row_move),
		    gtk_marshal_NONE__INT_INT,
		    GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);
  hlist_signals[CLICK_COLUMN] =
    gtk_signal_new ("click_column",
		    GTK_RUN_FIRST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkHListClass, click_column),
		    gtk_marshal_NONE__INT,
		    GTK_TYPE_NONE, 1, GTK_TYPE_INT);
  hlist_signals[RESIZE_COLUMN] =
    gtk_signal_new ("resize_column",
		    GTK_RUN_LAST,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkHListClass, resize_column),
		    gtk_marshal_NONE__INT_INT,
		    GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);

  hlist_signals[TOGGLE_FOCUS_ROW] =
    gtk_signal_new ("toggle_focus_row",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkHListClass, toggle_focus_row),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE, 0);
  hlist_signals[SELECT_ALL] =
    gtk_signal_new ("select_all",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkHListClass, select_all),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE, 0);
  hlist_signals[UNSELECT_ALL] =
    gtk_signal_new ("unselect_all",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkHListClass, unselect_all),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE, 0);
  hlist_signals[UNDO_SELECTION] =
    gtk_signal_new ("undo_selection",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkHListClass, undo_selection),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  hlist_signals[START_SELECTION] =
    gtk_signal_new ("start_selection",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkHListClass, start_selection),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  hlist_signals[END_SELECTION] =
    gtk_signal_new ("end_selection",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkHListClass, end_selection),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  hlist_signals[TOGGLE_ADD_MODE] =
    gtk_signal_new ("toggle_add_mode",
		    GTK_RUN_LAST | GTK_RUN_ACTION,
		    object_class->type,
		    GTK_SIGNAL_OFFSET (GtkHListClass, toggle_add_mode),
		    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);
  hlist_signals[EXTEND_SELECTION] =
    gtk_signal_new ("extend_selection",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkHListClass, extend_selection),
                    gtk_marshal_NONE__ENUM_FLOAT_BOOL,
                    GTK_TYPE_NONE, 3,
		    GTK_TYPE_SCROLL_TYPE, GTK_TYPE_FLOAT, GTK_TYPE_BOOL);
  hlist_signals[SCROLL_VERTICAL] =
    gtk_signal_new ("scroll_vertical",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkHListClass, scroll_vertical),
                    gtk_marshal_NONE__ENUM_FLOAT,
                    GTK_TYPE_NONE, 2, GTK_TYPE_SCROLL_TYPE, GTK_TYPE_FLOAT);
  hlist_signals[SCROLL_HORIZONTAL] =
    gtk_signal_new ("scroll_horizontal",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkHListClass, scroll_horizontal),
                    gtk_marshal_NONE__ENUM_FLOAT,
                    GTK_TYPE_NONE, 2, GTK_TYPE_SCROLL_TYPE, GTK_TYPE_FLOAT);
  hlist_signals[ABORT_COLUMN_RESIZE] =
    gtk_signal_new ("abort_column_resize",
                    GTK_RUN_LAST | GTK_RUN_ACTION,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (GtkHListClass, abort_column_resize),
                    gtk_marshal_NONE__NONE,
                    GTK_TYPE_NONE, 0);
  gtk_object_class_add_signals (object_class, hlist_signals, LAST_SIGNAL);

  widget_class->realize = gtk_hlist_realize;
  widget_class->unrealize = gtk_hlist_unrealize;
  widget_class->map = gtk_hlist_map;
  widget_class->unmap = gtk_hlist_unmap;
  widget_class->draw = gtk_hlist_draw;
  widget_class->button_press_event = gtk_hlist_button_press;
  widget_class->button_release_event = gtk_hlist_button_release;
  widget_class->motion_notify_event = gtk_hlist_motion;
  widget_class->expose_event = gtk_hlist_expose;
  widget_class->size_request = gtk_hlist_size_request;
  widget_class->size_allocate = gtk_hlist_size_allocate;
  widget_class->key_press_event = gtk_hlist_key_press;
  widget_class->focus_in_event = gtk_hlist_focus_in;
  widget_class->focus_out_event = gtk_hlist_focus_out;
  widget_class->draw_focus = gtk_hlist_draw_focus;
  widget_class->style_set = gtk_hlist_style_set;
  widget_class->drag_begin = gtk_hlist_drag_begin;
  widget_class->drag_end = gtk_hlist_drag_end;
  widget_class->drag_motion = gtk_hlist_drag_motion;
  widget_class->drag_leave = gtk_hlist_drag_leave;
  widget_class->drag_drop = gtk_hlist_drag_drop;
  widget_class->drag_data_get = gtk_hlist_drag_data_get;
  widget_class->drag_data_received = gtk_hlist_drag_data_received;

  /* container_class->add = NULL; use the default GtkContainerClass warning */
  /* container_class->remove=NULL; use the default GtkContainerClass warning */

  container_class->forall = gtk_hlist_forall;
  container_class->focus = gtk_hlist_focus;
  container_class->set_focus_child = gtk_hlist_set_focus_child;

  klass->set_scroll_adjustments = gtk_hlist_set_scroll_adjustments;
  klass->refresh = hlist_refresh;
  klass->select_row = real_select_row;
  klass->unselect_row = real_unselect_row;
  klass->row_move = real_row_move;
  klass->undo_selection = real_undo_selection;
  klass->resync_selection = resync_selection;
  klass->selection_find = selection_find;
  klass->click_column = NULL;
  klass->resize_column = real_resize_column;
  klass->draw_row = draw_row;
  klass->draw_drag_highlight = draw_drag_highlight;
  klass->insert_row = real_insert_row;
  klass->remove_row = real_remove_row;
  klass->clear = real_clear;
  klass->sort_list = real_sort_list;
  klass->select_all = real_select_all;
  klass->unselect_all = real_unselect_all;
  klass->fake_unselect_all = fake_unselect_all;
  klass->scroll_horizontal = scroll_horizontal;
  klass->scroll_vertical = scroll_vertical;
  klass->extend_selection = extend_selection;
  klass->toggle_focus_row = toggle_focus_row;
  klass->toggle_add_mode = toggle_add_mode;
  klass->start_selection = start_selection;
  klass->end_selection = end_selection;
  klass->abort_column_resize = abort_column_resize;
  klass->set_cell_contents = set_cell_contents;
  klass->cell_size_request = cell_size_request;

  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_Up, 0,
				"scroll_vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_Down, 0,
				"scroll_vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_Page_Up, 0,
				"scroll_vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_PAGE_BACKWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_Page_Down, 0,
				"scroll_vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_PAGE_FORWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_Home, GDK_CONTROL_MASK,
				"scroll_vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_End, GDK_CONTROL_MASK,
				"scroll_vertical", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 1.0);

  gtk_binding_entry_add_signal (binding_set, GDK_Up, GDK_SHIFT_MASK,
				"extend_selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
				GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_Down, GDK_SHIFT_MASK,
				"extend_selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
				GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_Page_Up, GDK_SHIFT_MASK,
				"extend_selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_PAGE_BACKWARD,
				GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_Page_Down, GDK_SHIFT_MASK,
				"extend_selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_PAGE_FORWARD,
				GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_Home,
				GDK_SHIFT_MASK | GDK_CONTROL_MASK,
				"extend_selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 0.0, GTK_TYPE_BOOL, TRUE);
  gtk_binding_entry_add_signal (binding_set, GDK_End,
				GDK_SHIFT_MASK | GDK_CONTROL_MASK,
				"extend_selection", 3,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 1.0, GTK_TYPE_BOOL, TRUE);

  gtk_binding_entry_add_signal (binding_set, GDK_Left, 0,
				"scroll_horizontal", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_Right, 0,
				"scroll_horizontal", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_Home, 0,
				"scroll_horizontal", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 0.0);
  gtk_binding_entry_add_signal (binding_set, GDK_End, 0,
				"scroll_horizontal", 2,
				GTK_TYPE_ENUM, GTK_SCROLL_JUMP,
				GTK_TYPE_FLOAT, 1.0);

  gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0,
				"undo_selection", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0,
				"abort_column_resize", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_space, 0,
				"toggle_focus_row", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_space, GDK_CONTROL_MASK,
				"toggle_add_mode", 0);
  gtk_binding_entry_add_signal (binding_set, '/', GDK_CONTROL_MASK,
				"select_all", 0);
  gtk_binding_entry_add_signal (binding_set, '\\', GDK_CONTROL_MASK,
				"unselect_all", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_Shift_L,
				GDK_RELEASE_MASK | GDK_SHIFT_MASK,
				"end_selection", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_Shift_R,
				GDK_RELEASE_MASK | GDK_SHIFT_MASK,
				"end_selection", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_Shift_L,
				GDK_RELEASE_MASK | GDK_SHIFT_MASK |
				GDK_CONTROL_MASK,
				"end_selection", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_Shift_R,
				GDK_RELEASE_MASK | GDK_SHIFT_MASK |
				GDK_CONTROL_MASK,
				"end_selection", 0);
}

static void
gtk_hlist_set_arg (GtkObject      *object,
		   GtkArg         *arg,
		   guint           arg_id)
{
  GtkHList *hlist;

  hlist = GTK_HLIST (object);

  switch (arg_id)
    {
    case ARG_N_COLUMNS: /* construct-only arg, only set when !GTK_CONSTRUCTED */
      gtk_hlist_construct (hlist, MAX (1, GTK_VALUE_UINT (*arg)), NULL);
      break;
    case ARG_SHADOW_TYPE:
      gtk_hlist_set_shadow_type (hlist, GTK_VALUE_ENUM (*arg));
      break;
    case ARG_SELECTION_MODE:
      gtk_hlist_set_selection_mode (hlist, GTK_VALUE_ENUM (*arg));
      break;
    case ARG_ROW_HEIGHT:
      gtk_hlist_set_row_height (hlist, GTK_VALUE_UINT (*arg));
      break;
    case ARG_REORDERABLE:
      gtk_hlist_set_reorderable (hlist, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_TITLES_ACTIVE:
      if (GTK_VALUE_BOOL (*arg))
	gtk_hlist_column_titles_active (hlist);
      else
	gtk_hlist_column_titles_passive (hlist);
      break;
    case ARG_USE_DRAG_ICONS:
      gtk_hlist_set_use_drag_icons (hlist, GTK_VALUE_BOOL (*arg));
      break;
    case ARG_SORT_TYPE:
      gtk_hlist_set_sort_type (hlist, GTK_VALUE_ENUM (*arg));
      break;
    }
}

static void
gtk_hlist_get_arg (GtkObject      *object,
		   GtkArg         *arg,
		   guint           arg_id)
{
  GtkHList *hlist;

  hlist = GTK_HLIST (object);

  switch (arg_id)
    {
      gint i;

    case ARG_N_COLUMNS:
      GTK_VALUE_UINT (*arg) = hlist->columns;
      break;
    case ARG_SHADOW_TYPE:
      GTK_VALUE_ENUM (*arg) = hlist->shadow_type;
      break;
    case ARG_SELECTION_MODE:
      GTK_VALUE_ENUM (*arg) = hlist->selection_mode;
      break;
    case ARG_ROW_HEIGHT:
      GTK_VALUE_UINT (*arg) = GTK_HLIST_ROW_HEIGHT_SET(hlist) ? hlist->row_height[0] : 0;
      break;
    case ARG_REORDERABLE:
      GTK_VALUE_BOOL (*arg) = GTK_HLIST_REORDERABLE (hlist);
      break;
    case ARG_TITLES_ACTIVE:
      GTK_VALUE_BOOL (*arg) = TRUE;
      for (i = 0; i < hlist->columns; i++)
	if (hlist->column[i].button &&
	    !GTK_WIDGET_SENSITIVE (hlist->column[i].button))
	  {
	    GTK_VALUE_BOOL (*arg) = FALSE;
	    break;
	  }
      break;
    case ARG_USE_DRAG_ICONS:
      GTK_VALUE_BOOL (*arg) = GTK_HLIST_USE_DRAG_ICONS (hlist);
      break;
    case ARG_SORT_TYPE:
      GTK_VALUE_ENUM (*arg) = hlist->sort_type;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
gtk_hlist_init (GtkHList *hlist)
{
  guint i;

  hlist->flags = 0;

  GTK_WIDGET_UNSET_FLAGS (hlist, GTK_NO_WINDOW);
  GTK_WIDGET_SET_FLAGS (hlist, GTK_CAN_FOCUS);
  GTK_HLIST_SET_FLAG (hlist, HLIST_CHILD_HAS_FOCUS);
  GTK_HLIST_SET_FLAG (hlist, HLIST_DRAW_DRAG_LINE);
  GTK_HLIST_SET_FLAG (hlist, HLIST_USE_DRAG_ICONS);

  hlist->row_mem_chunk = NULL;
  hlist->cell_mem_chunk = NULL;

  hlist->freeze_count = 0;

  hlist->rows = 0;
  hlist->row_center_offset = 0;
#define ROW_HEIGHT_BLOCKSIZE 16
  hlist->row_height = g_malloc(sizeof(guint)*ROW_HEIGHT_BLOCKSIZE);
  for (i = 0; i < ROW_HEIGHT_BLOCKSIZE; i++)
	hlist->row_height[i] = 0;
  hlist->row_list = NULL;
  hlist->row_list_end = NULL;

  hlist->columns = 0;

  hlist->title_window = NULL;
  hlist->column_title_area.x = 0;
  hlist->column_title_area.y = 0;
  hlist->column_title_area.width = 1;
  hlist->column_title_area.height = 1;

  hlist->hlist_window = NULL;
  hlist->hlist_window_width = 1;
  hlist->hlist_window_height = 1;

  hlist->hoffset = 0;
  hlist->voffset = 0;

  hlist->shadow_type = GTK_SHADOW_IN;
  hlist->vadjustment = NULL;
  hlist->hadjustment = NULL;

  hlist->button_actions[0] = GTK_BUTTON_SELECTS | GTK_BUTTON_DRAGS;
  hlist->button_actions[1] = GTK_BUTTON_IGNORED;
  hlist->button_actions[2] = GTK_BUTTON_IGNORED;
  hlist->button_actions[3] = GTK_BUTTON_IGNORED;
  hlist->button_actions[4] = GTK_BUTTON_IGNORED;

  hlist->cursor_drag = NULL;
  hlist->xor_gc = NULL;
  hlist->fg_gc = NULL;
  hlist->bg_gc = NULL;
  hlist->x_drag = 0;

  hlist->selection_mode = GTK_SELECTION_SINGLE;
  hlist->selection = NULL;
  hlist->selection_end = NULL;
  hlist->undo_selection = NULL;
  hlist->undo_unselection = NULL;

  hlist->focus_row = -1;
  hlist->undo_anchor = -1;

  hlist->anchor = -1;
  hlist->anchor_state = GTK_STATE_SELECTED;
  hlist->drag_pos = -1;
  hlist->htimer = 0;
  hlist->vtimer = 0;

  hlist->click_cell.row = -1;
  hlist->click_cell.column = -1;

  hlist->compare = default_compare;
  hlist->sort_type = GTK_SORT_ASCENDING;
  hlist->sort_column = 0;
}

/* Constructors */
void
gtk_hlist_construct (GtkHList *hlist,
		     gint      columns,
		     gchar    *titles[])
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));
  g_return_if_fail (columns > 0);
  g_return_if_fail (GTK_OBJECT_CONSTRUCTED (hlist) == FALSE);

  /* mark the object as constructed */
  gtk_object_constructed (GTK_OBJECT (hlist));

  /* initalize memory chunks, if this has not been done by any
   * possibly derived widget
   */
  if (!hlist->row_mem_chunk)
    hlist->row_mem_chunk = g_mem_chunk_new ("hlist row mem chunk",
					    sizeof (GtkHListRow),
					    sizeof (GtkHListRow) *
					    HLIST_OPTIMUM_SIZE, 
					    G_ALLOC_AND_FREE);

  if (!hlist->cell_mem_chunk)
    hlist->cell_mem_chunk = g_mem_chunk_new ("hlist cell mem chunk",
					     sizeof (GtkHell) * columns,
					     sizeof (GtkHell) * columns *
					     HLIST_OPTIMUM_SIZE, 
					     G_ALLOC_AND_FREE);

  /* set number of columns, allocate memory */
  hlist->columns = columns;
  hlist->column = columns_new (hlist);

  /* there needs to be at least one column button 
   * because there is alot of code that will break if it
   * isn't there*/
  column_button_create (hlist, 0);

  if (titles)
    {
      gint i;
      
      GTK_HLIST_SET_FLAG (hlist, HLIST_SHOW_TITLES);
      for (i = 0; i < columns; i++)
	gtk_hlist_set_column_title (hlist, i, titles[i]);
    }
  else
    {
      GTK_HLIST_UNSET_FLAG (hlist, HLIST_SHOW_TITLES);
    }
}

/* GTKHLIST PUBLIC INTERFACE
 *   gtk_hlist_new
 *   gtk_hlist_new_with_titles
 *   gtk_hlist_set_hadjustment
 *   gtk_hlist_set_vadjustment
 *   gtk_hlist_get_hadjustment
 *   gtk_hlist_get_vadjustment
 *   gtk_hlist_set_shadow_type
 *   gtk_hlist_set_selection_mode
 *   gtk_hlist_freeze
 *   gtk_hlist_thaw
 */
GtkWidget*
gtk_hlist_new (gint columns)
{
  return gtk_hlist_new_with_titles (columns, NULL);
}
 
GtkWidget*
gtk_hlist_new_with_titles (gint   columns,
			   gchar *titles[])
{
  GtkWidget *widget;

  widget = gtk_type_new (GTK_TYPE_HLIST);
  gtk_hlist_construct (GTK_HLIST (widget), columns, titles);

  return widget;
}

void
gtk_hlist_set_hadjustment (GtkHList      *hlist,
			   GtkAdjustment *adjustment)
{
  GtkAdjustment *old_adjustment;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));
  if (adjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  
  if (hlist->hadjustment == adjustment)
    return;
  
  old_adjustment = hlist->hadjustment;

  if (hlist->hadjustment)
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (hlist->hadjustment), hlist);
      gtk_object_unref (GTK_OBJECT (hlist->hadjustment));
    }

  hlist->hadjustment = adjustment;

  if (hlist->hadjustment)
    {
      gtk_object_ref (GTK_OBJECT (hlist->hadjustment));
      gtk_object_sink (GTK_OBJECT (hlist->hadjustment));

      gtk_signal_connect (GTK_OBJECT (hlist->hadjustment), "changed",
			  (GtkSignalFunc) hadjustment_changed,
			  (gpointer) hlist);
      gtk_signal_connect (GTK_OBJECT (hlist->hadjustment), "value_changed",
			  (GtkSignalFunc) hadjustment_value_changed,
			  (gpointer) hlist);
    }

  if (!hlist->hadjustment || !old_adjustment)
    gtk_widget_queue_resize (GTK_WIDGET (hlist));
}

GtkAdjustment *
gtk_hlist_get_hadjustment (GtkHList *hlist)
{
  g_return_val_if_fail (hlist != NULL, NULL);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), NULL);

  return hlist->hadjustment;
}

void
gtk_hlist_set_vadjustment (GtkHList      *hlist,
			   GtkAdjustment *adjustment)
{
  GtkAdjustment *old_adjustment;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));
  if (adjustment)
    g_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));

  if (hlist->vadjustment == adjustment)
    return;
  
  old_adjustment = hlist->vadjustment;

  if (hlist->vadjustment)
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (hlist->vadjustment), hlist);
      gtk_object_unref (GTK_OBJECT (hlist->vadjustment));
    }

  hlist->vadjustment = adjustment;

  if (hlist->vadjustment)
    {
      gtk_object_ref (GTK_OBJECT (hlist->vadjustment));
      gtk_object_sink (GTK_OBJECT (hlist->vadjustment));

      gtk_signal_connect (GTK_OBJECT (hlist->vadjustment), "changed",
			  (GtkSignalFunc) vadjustment_changed,
			  (gpointer) hlist);
      gtk_signal_connect (GTK_OBJECT (hlist->vadjustment), "value_changed",
			  (GtkSignalFunc) vadjustment_value_changed,
			  (gpointer) hlist);
    }

  if (!hlist->vadjustment || !old_adjustment)
    gtk_widget_queue_resize (GTK_WIDGET (hlist));
}

GtkAdjustment *
gtk_hlist_get_vadjustment (GtkHList *hlist)
{
  g_return_val_if_fail (hlist != NULL, NULL);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), NULL);

  return hlist->vadjustment;
}

static void
gtk_hlist_set_scroll_adjustments (GtkHList      *hlist,
				  GtkAdjustment *hadjustment,
				  GtkAdjustment *vadjustment)
{
  if (hlist->hadjustment != hadjustment)
    gtk_hlist_set_hadjustment (hlist, hadjustment);
  if (hlist->vadjustment != vadjustment)
    gtk_hlist_set_vadjustment (hlist, vadjustment);
}

void
gtk_hlist_set_shadow_type (GtkHList      *hlist,
			   GtkShadowType  type)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  hlist->shadow_type = type;

  if (GTK_WIDGET_VISIBLE (hlist))
    gtk_widget_queue_resize (GTK_WIDGET (hlist));
}

void
gtk_hlist_set_selection_mode (GtkHList         *hlist,
			      GtkSelectionMode  mode)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (mode == hlist->selection_mode)
    return;

  hlist->selection_mode = mode;
  hlist->anchor = -1;
  hlist->anchor_state = GTK_STATE_SELECTED;
  hlist->drag_pos = -1;
  hlist->undo_anchor = hlist->focus_row;

  g_list_free (hlist->undo_selection);
  g_list_free (hlist->undo_unselection);
  hlist->undo_selection = NULL;
  hlist->undo_unselection = NULL;

  switch (mode)
    {
    case GTK_SELECTION_MULTIPLE:
    case GTK_SELECTION_EXTENDED:
      return;
    case GTK_SELECTION_BROWSE:
    case GTK_SELECTION_SINGLE:
      gtk_hlist_unselect_all (hlist);
      break;
    }
}

void
gtk_hlist_freeze (GtkHList *hlist)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  hlist->freeze_count++;
}

void
gtk_hlist_thaw (GtkHList *hlist)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (hlist->freeze_count)
    {
      hlist->freeze_count--;
      HLIST_REFRESH (hlist);
    }
}

/* PUBLIC COLUMN FUNCTIONS
 *   gtk_hlist_column_titles_show
 *   gtk_hlist_column_titles_hide
 *   gtk_hlist_column_title_active
 *   gtk_hlist_column_title_passive
 *   gtk_hlist_column_titles_active
 *   gtk_hlist_column_titles_passive
 *   gtk_hlist_set_column_title
 *   gtk_hlist_get_column_title
 *   gtk_hlist_set_column_widget
 *   gtk_hlist_set_column_justification
 *   gtk_hlist_set_column_visibility
 *   gtk_hlist_set_column_resizeable
 *   gtk_hlist_set_column_auto_resize
 *   gtk_hlist_optimal_column_width
 *   gtk_hlist_set_column_width
 *   gtk_hlist_set_column_min_width
 *   gtk_hlist_set_column_max_width
 */
void
gtk_hlist_column_titles_show (GtkHList *hlist)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (!GTK_HLIST_SHOW_TITLES(hlist))
    {
      GTK_HLIST_SET_FLAG (hlist, HLIST_SHOW_TITLES);
      if (hlist->title_window)
	gdk_window_show (hlist->title_window);
      gtk_widget_queue_resize (GTK_WIDGET (hlist));
    }
}

void 
gtk_hlist_column_titles_hide (GtkHList *hlist)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (GTK_HLIST_SHOW_TITLES(hlist))
    {
      GTK_HLIST_UNSET_FLAG (hlist, HLIST_SHOW_TITLES);
      if (hlist->title_window)
	gdk_window_hide (hlist->title_window);
      gtk_widget_queue_resize (GTK_WIDGET (hlist));
    }
}

void
gtk_hlist_column_title_active (GtkHList *hlist,
			       gint      column)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (column < 0 || column >= hlist->columns)
    return;
  if (!hlist->column[column].button || !hlist->column[column].button_passive)
    return;

  hlist->column[column].button_passive = FALSE;

  gtk_signal_disconnect_by_func (GTK_OBJECT (hlist->column[column].button),
				 (GtkSignalFunc) column_title_passive_func,
				 NULL);

  GTK_WIDGET_SET_FLAGS (hlist->column[column].button, GTK_CAN_FOCUS);
  if (GTK_WIDGET_VISIBLE (hlist))
    gtk_widget_queue_draw (hlist->column[column].button);
}

void
gtk_hlist_column_title_passive (GtkHList *hlist,
				gint      column)
{
  GtkButton *button;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (column < 0 || column >= hlist->columns)
    return;
  if (!hlist->column[column].button || hlist->column[column].button_passive)
    return;

  button = GTK_BUTTON (hlist->column[column].button);

  hlist->column[column].button_passive = TRUE;

  if (button->button_down)
    gtk_button_released (button);
  if (button->in_button)
    gtk_button_leave (button);

  gtk_signal_connect (GTK_OBJECT (hlist->column[column].button), "event",
		      (GtkSignalFunc) column_title_passive_func, NULL);

  GTK_WIDGET_UNSET_FLAGS (hlist->column[column].button, GTK_CAN_FOCUS);
  if (GTK_WIDGET_VISIBLE (hlist))
    gtk_widget_queue_draw (hlist->column[column].button);
}

void
gtk_hlist_column_titles_active (GtkHList *hlist)
{
  gint i;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (!GTK_HLIST_SHOW_TITLES(hlist))
    return;

  for (i = 0; i < hlist->columns; i++)
    gtk_hlist_column_title_active (hlist, i);
}

void
gtk_hlist_column_titles_passive (GtkHList *hlist)
{
  gint i;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (!GTK_HLIST_SHOW_TITLES(hlist))
    return;

  for (i = 0; i < hlist->columns; i++)
    gtk_hlist_column_title_passive (hlist, i);
}

void
gtk_hlist_set_column_title (GtkHList    *hlist,
			    gint         column,
			    const gchar *title)
{
  gint new_button = 0;
  GtkWidget *old_widget;
  GtkWidget *alignment = NULL;
  GtkWidget *label;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (column < 0 || column >= hlist->columns)
    return;

  /* if the column button doesn't currently exist,
   * it has to be created first */
  if (!hlist->column[column].button)
    {
      column_button_create (hlist, column);
      new_button = 1;
    }

  column_title_new (hlist, column, title);

  /* remove and destroy the old widget */
  old_widget = GTK_BIN (hlist->column[column].button)->child;
  if (old_widget)
    gtk_container_remove (GTK_CONTAINER (hlist->column[column].button), old_widget);

  /* create new alignment based no column justification */
  switch (hlist->column[column].justification)
    {
    case GTK_JUSTIFY_LEFT:
      alignment = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
      break;

    case GTK_JUSTIFY_RIGHT:
      alignment = gtk_alignment_new (1.0, 0.5, 0.0, 0.0);
      break;

    case GTK_JUSTIFY_CENTER:
      alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
      break;

    case GTK_JUSTIFY_FILL:
      alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
      break;
    }

  gtk_widget_push_composite_child ();
  label = gtk_label_new (hlist->column[column].title);
  gtk_widget_pop_composite_child ();
  gtk_container_add (GTK_CONTAINER (alignment), label);
  gtk_container_add (GTK_CONTAINER (hlist->column[column].button), alignment);
  gtk_widget_show (label);
  gtk_widget_show (alignment);

  /* if this button didn't previously exist, then the
   * column button positions have to be re-computed */
  if (GTK_WIDGET_VISIBLE (hlist) && new_button)
    size_allocate_title_buttons (hlist);
}

gchar *
gtk_hlist_get_column_title (GtkHList *hlist,
			    gint      column)
{
  g_return_val_if_fail (hlist != NULL, NULL);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), NULL);

  if (column < 0 || column >= hlist->columns)
    return NULL;

  return hlist->column[column].title;
}

void
gtk_hlist_set_column_widget (GtkHList  *hlist,
			     gint       column,
			     GtkWidget *widget)
{
  gint new_button = 0;
  GtkWidget *old_widget;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (column < 0 || column >= hlist->columns)
    return;

  /* if the column button doesn't currently exist,
   * it has to be created first */
  if (!hlist->column[column].button)
    {
      column_button_create (hlist, column);
      new_button = 1;
    }

  column_title_new (hlist, column, NULL);

  /* remove and destroy the old widget */
  old_widget = GTK_BIN (hlist->column[column].button)->child;
  if (old_widget)
    gtk_container_remove (GTK_CONTAINER (hlist->column[column].button),
			  old_widget);

  /* add and show the widget */
  if (widget)
    {
      gtk_container_add (GTK_CONTAINER (hlist->column[column].button), widget);
      gtk_widget_show (widget);
    }

  /* if this button didn't previously exist, then the
   * column button positions have to be re-computed */
  if (GTK_WIDGET_VISIBLE (hlist) && new_button)
    size_allocate_title_buttons (hlist);
}

GtkWidget *
gtk_hlist_get_column_widget (GtkHList *hlist,
			     gint      column)
{
  g_return_val_if_fail (hlist != NULL, NULL);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), NULL);

  if (column < 0 || column >= hlist->columns)
    return NULL;

  if (hlist->column[column].button)
    return GTK_BUTTON (hlist->column[column].button)->child;

  return NULL;
}

void
gtk_hlist_set_column_justification (GtkHList         *hlist,
				    gint              column,
				    GtkJustification  justification)
{
  GtkWidget *alignment;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (column < 0 || column >= hlist->columns)
    return;

  hlist->column[column].justification = justification;

  /* change the alinment of the button title if it's not a
   * custom widget */
  if (hlist->column[column].title)
    {
      alignment = GTK_BIN (hlist->column[column].button)->child;

      switch (hlist->column[column].justification)
	{
	case GTK_JUSTIFY_LEFT:
	  gtk_alignment_set (GTK_ALIGNMENT (alignment), 0.0, 0.5, 0.0, 0.0);
	  break;

	case GTK_JUSTIFY_RIGHT:
	  gtk_alignment_set (GTK_ALIGNMENT (alignment), 1.0, 0.5, 0.0, 0.0);
	  break;

	case GTK_JUSTIFY_CENTER:
	  gtk_alignment_set (GTK_ALIGNMENT (alignment), 0.5, 0.5, 0.0, 0.0);
	  break;

	case GTK_JUSTIFY_FILL:
	  gtk_alignment_set (GTK_ALIGNMENT (alignment), 0.5, 0.5, 0.0, 0.0);
	  break;

	default:
	  break;
	}
    }

  if (HLIST_UNFROZEN (hlist))
    draw_rows (hlist, NULL);
}

void
gtk_hlist_set_column_visibility (GtkHList *hlist,
				 gint      column,
				 gboolean  visible)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (column < 0 || column >= hlist->columns)
    return;
  if (hlist->column[column].visible == visible)
    return;

  /* don't hide last visible column */
  if (!visible)
    {
      gint i;
      gint vis_columns = 0;

      for (i = 0, vis_columns = 0; i < hlist->columns && vis_columns < 2; i++)
	if (hlist->column[i].visible)
	  vis_columns++;

      if (vis_columns < 2)
	return;
    }

  hlist->column[column].visible = visible;

  if (hlist->column[column].button)
    {
      if (visible)
	gtk_widget_show (hlist->column[column].button);
      else
	gtk_widget_hide (hlist->column[column].button);
    }
  
  gtk_widget_queue_resize (GTK_WIDGET(hlist));
}

void
gtk_hlist_set_column_resizeable (GtkHList *hlist,
				 gint      column,
				 gboolean  resizeable)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (column < 0 || column >= hlist->columns)
    return;
  if (hlist->column[column].resizeable == resizeable)
    return;

  hlist->column[column].resizeable = resizeable;
  if (resizeable)
    hlist->column[column].auto_resize = FALSE;

  if (GTK_WIDGET_VISIBLE (hlist))
    size_allocate_title_buttons (hlist);
}

void
gtk_hlist_set_column_auto_resize (GtkHList *hlist,
				  gint      column,
				  gboolean  auto_resize)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (column < 0 || column >= hlist->columns)
    return;
  if (hlist->column[column].auto_resize == auto_resize)
    return;

  hlist->column[column].auto_resize = auto_resize;
  if (auto_resize)
    {
      hlist->column[column].resizeable = FALSE;
      if (!GTK_HLIST_AUTO_RESIZE_BLOCKED(hlist))
	{
	  gint width;

	  width = gtk_hlist_optimal_column_width (hlist, column);
	  gtk_hlist_set_column_width (hlist, column, width);
	}
    }

  if (GTK_WIDGET_VISIBLE (hlist))
    size_allocate_title_buttons (hlist);
}

gint
gtk_hlist_columns_autosize (GtkHList *hlist)
{
  gint i;
  gint width;

  g_return_val_if_fail (hlist != NULL, 0);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), 0);

  gtk_hlist_freeze (hlist);
  width = 0;
  for (i = 0; i < hlist->columns; i++)
    {
      gtk_hlist_set_column_width (hlist, i,
				  gtk_hlist_optimal_column_width (hlist, i));

      width += hlist->column[i].width;
    }

  gtk_hlist_thaw (hlist);
  return width;
}

gint
gtk_hlist_optimal_column_width (GtkHList *hlist,
				gint      column)
{
  GtkRequisition requisition;
  GList *list;
  gint width;

  g_return_val_if_fail (hlist != NULL, 0);
  g_return_val_if_fail (GTK_HLIST (hlist), 0);

  if (column < 0 || column > hlist->columns)
    return 0;

  if (GTK_HLIST_SHOW_TITLES(hlist) && hlist->column[column].button)
    width = (hlist->column[column].button->requisition.width)
#if 0
	     (HELL_SPACING + (2 * COLUMN_INSET)))
#endif
		;
  else
    width = 0;

  for (list = hlist->row_list; list; list = list->next)
    {
      GTK_HLIST_CLASS_FW (hlist)->cell_size_request
	(hlist, GTK_HLIST_ROW (list), column, &requisition);
      width = MAX (width, requisition.width);
    }

  return width;
}

void
gtk_hlist_set_column_width (GtkHList *hlist,
			    gint      column,
			    gint      width)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (column < 0 || column >= hlist->columns)
    return;

  gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[RESIZE_COLUMN],
		   column, width);
}

void
gtk_hlist_set_column_min_width (GtkHList *hlist,
				gint      column,
				gint      min_width)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (column < 0 || column >= hlist->columns)
    return;
  if (hlist->column[column].min_width == min_width)
    return;

  if (hlist->column[column].max_width >= 0  &&
      hlist->column[column].max_width < min_width)
    hlist->column[column].min_width = hlist->column[column].max_width;
  else
    hlist->column[column].min_width = min_width;

  if (hlist->column[column].area.width < hlist->column[column].min_width)
    gtk_hlist_set_column_width (hlist, column,hlist->column[column].min_width);
}

void
gtk_hlist_set_column_max_width (GtkHList *hlist,
				gint      column,
				gint      max_width)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (column < 0 || column >= hlist->columns)
    return;
  if (hlist->column[column].max_width == max_width)
    return;

  if (hlist->column[column].min_width >= 0 && max_width >= 0 &&
      hlist->column[column].min_width > max_width)
    hlist->column[column].max_width = hlist->column[column].min_width;
  else
    hlist->column[column].max_width = max_width;
  
  if (hlist->column[column].area.width > hlist->column[column].max_width)
    gtk_hlist_set_column_width (hlist, column,hlist->column[column].max_width);
}

/* PRIVATE COLUMN FUNCTIONS
 *   column_auto_resize
 *   real_resize_column
 *   abort_column_resize
 *   size_allocate_title_buttons
 *   size_allocate_columns
 *   list_requisition_width
 *   new_column_width
 *   column_button_create
 *   column_button_clicked
 *   column_title_passive_func
 */
static void
column_auto_resize (GtkHList    *hlist,
		    GtkHListRow *hlist_row,
		    gint         column,
		    gint         old_width)
{
  /* resize column if needed for auto_resize */
  GtkRequisition requisition;

  if (!hlist->column[column].auto_resize ||
      GTK_HLIST_AUTO_RESIZE_BLOCKED(hlist))
    return;

  if (hlist_row)
    GTK_HLIST_CLASS_FW (hlist)->cell_size_request (hlist, hlist_row,
						   column, &requisition);
  else
    requisition.width = 0;

  if (requisition.width > hlist->column[column].width)
    gtk_hlist_set_column_width (hlist, column, requisition.width);
  else if (requisition.width < old_width &&
	   old_width == hlist->column[column].width)
    {
      GList *list;
      gint new_width = 0;

      /* run a "gtk_hlist_optimal_column_width" but break, if
       * the column doesn't shrink */
      if (GTK_HLIST_SHOW_TITLES(hlist) && hlist->column[column].button)
	new_width = (hlist->column[column].button->requisition.width -
		     (HELL_SPACING + (2 * COLUMN_INSET)));
      else
	new_width = 0;

      for (list = hlist->row_list; list; list = list->next)
	{
	  GTK_HLIST_CLASS_FW (hlist)->cell_size_request
	    (hlist, GTK_HLIST_ROW (list), column, &requisition);
	  new_width = MAX (new_width, requisition.width);
	  if (new_width == hlist->column[column].width)
	    break;
	}
      if (new_width < hlist->column[column].width)
	gtk_hlist_set_column_width
	  (hlist, column, MAX (new_width, hlist->column[column].min_width));
    }
}

static void
real_resize_column (GtkHList *hlist,
		    gint      column,
		    gint      width)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (column < 0 || column >= hlist->columns)
    return;
  
  if (width < MAX (COLUMN_MIN_WIDTH, hlist->column[column].min_width))
    width = MAX (COLUMN_MIN_WIDTH, hlist->column[column].min_width);
  if (hlist->column[column].max_width >= 0 &&
      width > hlist->column[column].max_width)
    width = hlist->column[column].max_width;

  hlist->column[column].width = width;
  hlist->column[column].width_set = TRUE;

  /* FIXME: this is quite expensive to do if the widget hasn't
   *        been size_allocated yet, and pointless. Should
   *        a flag be kept
   */
  size_allocate_columns (hlist, TRUE);
  size_allocate_title_buttons (hlist);

  HLIST_REFRESH (hlist);
}

static void
abort_column_resize (GtkHList *hlist)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (!GTK_HLIST_IN_DRAG(hlist))
    return;

  GTK_HLIST_UNSET_FLAG (hlist, HLIST_IN_DRAG);
  gtk_grab_remove (GTK_WIDGET (hlist));
  gdk_pointer_ungrab (GDK_CURRENT_TIME);
  hlist->drag_pos = -1;

  if (hlist->x_drag >= 0 && hlist->x_drag <= hlist->hlist_window_width - 1)
    draw_xor_line (hlist);

  if (GTK_HLIST_ADD_MODE(hlist))
    {
      gdk_gc_set_line_attributes (hlist->xor_gc, 1, GDK_LINE_ON_OFF_DASH, 0,0);
      gdk_gc_set_dashes (hlist->xor_gc, 0, "\4\4", 2);
    }
}

static void
size_allocate_title_buttons (GtkHList *hlist)
{
  GtkAllocation button_allocation;
  gint last_column;
  gint last_button = 0;
  gint i;

  if (!GTK_WIDGET_REALIZED (hlist))
    return;

  button_allocation.x = hlist->hoffset;
  button_allocation.y = 0;
  button_allocation.width = 0;
  button_allocation.height = hlist->column_title_area.height;

  /* find last visible column */
  for (last_column = hlist->columns - 1; last_column >= 0; last_column--)
    if (hlist->column[last_column].visible)
      break;

  for (i = 0; i < last_column; i++)
    {
      if (!hlist->column[i].visible)
	{
	  last_button = i + 1;
	  gdk_window_hide (hlist->column[i].window);
	  continue;
	}

      button_allocation.width += (hlist->column[i].area.width +
				  HELL_SPACING + 2 * COLUMN_INSET);

      if (!hlist->column[i + 1].button)
	{
	  gdk_window_hide (hlist->column[i].window);
	  continue;
	}

      gtk_widget_size_allocate (hlist->column[last_button].button,
				&button_allocation);
      button_allocation.x += button_allocation.width;
      button_allocation.width = 0;

      if (hlist->column[last_button].resizeable)
	{
	  gdk_window_show (hlist->column[last_button].window);
	  gdk_window_move_resize (hlist->column[last_button].window,
				  button_allocation.x - (DRAG_WIDTH / 2), 
				  0, DRAG_WIDTH,
				  hlist->column_title_area.height);
	}
      else
	gdk_window_hide (hlist->column[last_button].window);

      last_button = i + 1;
    }

  button_allocation.width += (hlist->column[last_column].area.width +
			      2 * (HELL_SPACING + COLUMN_INSET));
  gtk_widget_size_allocate (hlist->column[last_button].button,
			    &button_allocation);

  if (hlist->column[last_button].resizeable)
    {
      button_allocation.x += button_allocation.width;

      gdk_window_show (hlist->column[last_button].window);
      gdk_window_move_resize (hlist->column[last_button].window,
			      button_allocation.x - (DRAG_WIDTH / 2), 
			      0, DRAG_WIDTH, hlist->column_title_area.height);
    }
  else
    gdk_window_hide (hlist->column[last_button].window);
}

static void
size_allocate_columns (GtkHList *hlist,
		       gboolean  block_resize)
{
  gint xoffset = HELL_SPACING + COLUMN_INSET;
  gint last_column;
  gint i;

  /* find last visible column and calculate correct column width */
  for (last_column = hlist->columns - 1;
       last_column >= 0 && !hlist->column[last_column].visible; last_column--);

  if (last_column < 0)
    return;

  for (i = 0; i <= last_column; i++)
    {
      if (!hlist->column[i].visible)
	continue;
      hlist->column[i].area.x = xoffset;
      if (hlist->column[i].width_set)
	{
	  if (!block_resize && GTK_HLIST_SHOW_TITLES(hlist) &&
	      hlist->column[i].auto_resize && hlist->column[i].button)
	    {
	      gint width;

	      width = (hlist->column[i].button->requisition.width -
		       (HELL_SPACING + (2 * COLUMN_INSET)));

	      if (width > hlist->column[i].width)
		gtk_hlist_set_column_width (hlist, i, width);
	    }

	  hlist->column[i].area.width = hlist->column[i].width;
	  xoffset += hlist->column[i].width + HELL_SPACING + (2* COLUMN_INSET);
	}
      else if (GTK_HLIST_SHOW_TITLES(hlist) && hlist->column[i].button)
	{
	  hlist->column[i].area.width =
	    hlist->column[i].button->requisition.width -
	    (HELL_SPACING + (2 * COLUMN_INSET));
	  xoffset += hlist->column[i].button->requisition.width;
	}
    }

  hlist->column[last_column].area.width = hlist->column[last_column].area.width
    + MAX (0, hlist->hlist_window_width + COLUMN_INSET - xoffset);
}

static gint
list_requisition_width (GtkHList *hlist) 
{
  gint width = HELL_SPACING;
  gint i;

  for (i = hlist->columns - 1; i >= 0; i--)
    {
      if (!hlist->column[i].visible)
	continue;

      if (hlist->column[i].width_set)
	width += hlist->column[i].width + HELL_SPACING + (2 * COLUMN_INSET);
      else if (GTK_HLIST_SHOW_TITLES(hlist) && hlist->column[i].button)
	width += hlist->column[i].button->requisition.width;
    }

  return width;
}

/* this function returns the new width of the column being resized given
 * the column and x position of the cursor; the x cursor position is passed
 * in as a pointer and automagicly corrected if it's beyond min/max limits */
static gint
new_column_width (GtkHList *hlist,
		  gint      column,
		  gint     *x)
{
  gint xthickness = GTK_WIDGET (hlist)->style->klass->xthickness;
  gint width;
  gint cx;
  gint dx;
  gint last_column;

  /* first translate the x position from widget->window
   * to hlist->hlist_window */
  cx = *x - xthickness;

  for (last_column = hlist->columns - 1;
       last_column >= 0 && !hlist->column[last_column].visible; last_column--);

  /* calculate new column width making sure it doesn't end up
   * less than the minimum width */
  dx = (COLUMN_LEFT_XPIXEL (hlist, column) + COLUMN_INSET +
	(column < last_column) * HELL_SPACING);
  width = cx - dx;

  if (width < MAX (COLUMN_MIN_WIDTH, hlist->column[column].min_width))
    {
      width = MAX (COLUMN_MIN_WIDTH, hlist->column[column].min_width);
      cx = dx + width;
      *x = cx + xthickness;
    }
  else if (hlist->column[column].max_width >= COLUMN_MIN_WIDTH &&
	   width > hlist->column[column].max_width)
    {
      width = hlist->column[column].max_width;
      cx = dx + hlist->column[column].max_width;
      *x = cx + xthickness;
    }      

  if (cx < 0 || cx > hlist->hlist_window_width)
    *x = -1;

  return width;
}

static void
column_button_create (GtkHList *hlist,
		      gint      column)
{
  GtkWidget *button;

  gtk_widget_push_composite_child ();
  button = hlist->column[column].button = gtk_button_new ();
  gtk_widget_pop_composite_child ();

  if (GTK_WIDGET_REALIZED (hlist) && hlist->title_window)
    gtk_widget_set_parent_window (hlist->column[column].button,
				  hlist->title_window);
  gtk_widget_set_parent (button, GTK_WIDGET (hlist));

  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      (GtkSignalFunc) column_button_clicked,
		      (gpointer) hlist);
  gtk_widget_show (button);
}

static void
column_button_clicked (GtkWidget *widget,
		       gpointer   data)
{
  gint i;
  GtkHList *hlist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HLIST (data));

  hlist = GTK_HLIST (data);

  /* find the column who's button was pressed */
  for (i = 0; i < hlist->columns; i++)
    if (hlist->column[i].button == widget)
      break;

  gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[CLICK_COLUMN], i);
}

static gint
column_title_passive_func (GtkWidget *widget, 
			   GdkEvent  *event,
			   gpointer   data)
{
  g_return_val_if_fail (event != NULL, FALSE);
  
  switch (event->type)
    {
    case GDK_MOTION_NOTIFY:
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      return TRUE;
    default:
      break;
    }
  return FALSE;
}


/* PUBLIC HELL FUNCTIONS
 *   gtk_hlist_get_cell_type
 *   gtk_hlist_set_text
 *   gtk_hlist_get_text
 *   gtk_hlist_set_pixmap
 *   gtk_hlist_get_pixmap
 *   gtk_hlist_set_pixtext
 *   gtk_hlist_get_pixtext
 *   gtk_hlist_set_shift
 */
GtkHellType 
gtk_hlist_get_cell_type (GtkHList *hlist,
			 gint      row,
			 gint      column)
{
  GtkHListRow *hlist_row;

  g_return_val_if_fail (hlist != NULL, -1);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), -1);

  if (row < 0 || row >= hlist->rows)
    return -1;
  if (column < 0 || column >= hlist->columns)
    return -1;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;

  return hlist_row->cell[column].type;
}

void
gtk_hlist_set_text (GtkHList    *hlist,
		    gint         row,
		    gint         column,
		    const gchar *text)
{
  GtkHListRow *hlist_row;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (row < 0 || row >= hlist->rows)
    return;
  if (column < 0 || column >= hlist->columns)
    return;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;

  /* if text is null, then the cell is empty */
  GTK_HLIST_CLASS_FW (hlist)->set_cell_contents
    (hlist, hlist_row, column, GTK_HELL_TEXT, text, 0, NULL, NULL);

  /* redraw the list if it's not frozen */
  if (HLIST_UNFROZEN (hlist))
    {
      if (gtk_hlist_row_is_visible (hlist, row) != GTK_VISIBILITY_NONE)
	GTK_HLIST_CLASS_FW (hlist)->draw_row (hlist, NULL, row, hlist_row);
    }
}

gint
gtk_hlist_get_text (GtkHList  *hlist,
		    gint       row,
		    gint       column,
		    gchar    **text)
{
  GtkHListRow *hlist_row;

  g_return_val_if_fail (hlist != NULL, 0);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), 0);

  if (row < 0 || row >= hlist->rows)
    return 0;
  if (column < 0 || column >= hlist->columns)
    return 0;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;

  if (hlist_row->cell[column].type != GTK_HELL_TEXT)
    return 0;

  if (text)
    *text = GTK_HELL_TEXT (hlist_row->cell[column])->text;

  return 1;
}

void
gtk_hlist_set_pixmap (GtkHList  *hlist,
		      gint       row,
		      gint       column,
		      GdkPixmap *pixmap,
		      GdkBitmap *mask)
{
  GtkHListRow *hlist_row;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (row < 0 || row >= hlist->rows)
    return;
  if (column < 0 || column >= hlist->columns)
    return;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;
  
  gdk_pixmap_ref (pixmap);
  
  if (mask) gdk_pixmap_ref (mask);
  
  GTK_HLIST_CLASS_FW (hlist)->set_cell_contents
    (hlist, hlist_row, column, GTK_HELL_PIXMAP, NULL, 0, pixmap, mask);

  /* redraw the list if it's not frozen */
  if (HLIST_UNFROZEN (hlist))
    {
      if (gtk_hlist_row_is_visible (hlist, row) != GTK_VISIBILITY_NONE)
	GTK_HLIST_CLASS_FW (hlist)->draw_row (hlist, NULL, row, hlist_row);
    }
}

gint
gtk_hlist_get_pixmap (GtkHList   *hlist,
		      gint        row,
		      gint        column,
		      GdkPixmap **pixmap,
		      GdkBitmap **mask)
{
  GtkHListRow *hlist_row;

  g_return_val_if_fail (hlist != NULL, 0);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), 0);

  if (row < 0 || row >= hlist->rows)
    return 0;
  if (column < 0 || column >= hlist->columns)
    return 0;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;

  if (hlist_row->cell[column].type != GTK_HELL_PIXMAP)
    return 0;

  if (pixmap)
  {
    *pixmap = GTK_HELL_PIXMAP (hlist_row->cell[column])->pixmap;
  }
  if (mask)
  {
    /* mask can be NULL */
    *mask = GTK_HELL_PIXMAP (hlist_row->cell[column])->mask;
  }

  return 1;
}

void
gtk_hlist_set_pixtext (GtkHList    *hlist,
		       gint         row,
		       gint         column,
		       const gchar *text,
		       guint8       spacing,
		       GdkPixmap   *pixmap,
		       GdkBitmap   *mask)
{
  GtkHListRow *hlist_row;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (row < 0 || row >= hlist->rows)
    return;
  if (column < 0 || column >= hlist->columns)
    return;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;
  
  gdk_pixmap_ref (pixmap);
  if (mask) gdk_pixmap_ref (mask);
  GTK_HLIST_CLASS_FW (hlist)->set_cell_contents
    (hlist, hlist_row, column, GTK_HELL_PIXTEXT, text, spacing, pixmap, mask);

  /* redraw the list if it's not frozen */
  if (HLIST_UNFROZEN (hlist))
    {
      if (gtk_hlist_row_is_visible (hlist, row) != GTK_VISIBILITY_NONE)
	GTK_HLIST_CLASS_FW (hlist)->draw_row (hlist, NULL, row, hlist_row);
    }
}

gint
gtk_hlist_get_pixtext (GtkHList   *hlist,
		       gint        row,
		       gint        column,
		       gchar     **text,
		       guint8     *spacing,
		       GdkPixmap **pixmap,
		       GdkBitmap **mask)
{
  GtkHListRow *hlist_row;

  g_return_val_if_fail (hlist != NULL, 0);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), 0);

  if (row < 0 || row >= hlist->rows)
    return 0;
  if (column < 0 || column >= hlist->columns)
    return 0;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;

  if (hlist_row->cell[column].type != GTK_HELL_PIXTEXT)
    return 0;

  if (text)
    *text = GTK_HELL_PIXTEXT (hlist_row->cell[column])->text;
  if (spacing)
    *spacing = GTK_HELL_PIXTEXT (hlist_row->cell[column])->spacing;
  if (pixmap)
    *pixmap = GTK_HELL_PIXTEXT (hlist_row->cell[column])->pixmap;

  /* mask can be NULL */
  if (mask)
    *mask = GTK_HELL_PIXTEXT (hlist_row->cell[column])->mask;

  return 1;
}

void
gtk_hlist_set_shift (GtkHList *hlist,
		     gint      row,
		     gint      column,
		     gint      vertical,
		     gint      horizontal)
{
  GtkRequisition requisition = { 0, 0 };
  GtkHListRow *hlist_row;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (row < 0 || row >= hlist->rows)
    return;
  if (column < 0 || column >= hlist->columns)
    return;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;

  if (hlist->column[column].auto_resize &&
      !GTK_HLIST_AUTO_RESIZE_BLOCKED(hlist))
    GTK_HLIST_CLASS_FW (hlist)->cell_size_request (hlist, hlist_row,
						   column, &requisition);

  hlist_row->cell[column].vertical = vertical;
  hlist_row->cell[column].horizontal = horizontal;

  column_auto_resize (hlist, hlist_row, column, requisition.width);

  if (HLIST_UNFROZEN (hlist) && gtk_hlist_row_is_visible (hlist, row) != GTK_VISIBILITY_NONE)
    GTK_HLIST_CLASS_FW (hlist)->draw_row (hlist, NULL, row, hlist_row);
}

/* PRIVATE HELL FUNCTIONS
 *   set_cell_contents
 *   cell_size_request
 */
static void
set_cell_contents (GtkHList    *hlist,
		   GtkHListRow *hlist_row,
		   gint         column,
		   GtkHellType  type,
		   const gchar *text,
		   guint8       spacing,
		   GdkPixmap   *pixmap,
		   GdkBitmap   *mask)
{
  GtkRequisition requisition;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));
  g_return_if_fail (hlist_row != NULL);

  if (hlist->column[column].auto_resize &&
      !GTK_HLIST_AUTO_RESIZE_BLOCKED(hlist))
    GTK_HLIST_CLASS_FW (hlist)->cell_size_request (hlist, hlist_row,
						   column, &requisition);

  switch (hlist_row->cell[column].type)
    {
    case GTK_HELL_EMPTY:
      break;
    case GTK_HELL_TEXT:
      g_free (GTK_HELL_TEXT (hlist_row->cell[column])->text);
      break;
    case GTK_HELL_PIXMAP:
      gdk_pixmap_unref (GTK_HELL_PIXMAP (hlist_row->cell[column])->pixmap);
      if (GTK_HELL_PIXMAP (hlist_row->cell[column])->mask)
	gdk_bitmap_unref (GTK_HELL_PIXMAP (hlist_row->cell[column])->mask);
      break;
    case GTK_HELL_PIXTEXT:
      g_free (GTK_HELL_PIXTEXT (hlist_row->cell[column])->text);
      gdk_pixmap_unref (GTK_HELL_PIXTEXT (hlist_row->cell[column])->pixmap);
      if (GTK_HELL_PIXTEXT (hlist_row->cell[column])->mask)
	gdk_bitmap_unref (GTK_HELL_PIXTEXT (hlist_row->cell[column])->mask);
      break;
    case GTK_HELL_WIDGET:
      /* unimplimented */
      break;
    default:
      break;
    }

  hlist_row->cell[column].type = GTK_HELL_EMPTY;

  switch (type)
    {
    case GTK_HELL_TEXT:
      if (text)
	{
	  hlist_row->cell[column].type = GTK_HELL_TEXT;
	  GTK_HELL_TEXT (hlist_row->cell[column])->text = g_strdup (text);
	}
      break;
    case GTK_HELL_PIXMAP:
      if (pixmap)
	{
	  hlist_row->cell[column].type = GTK_HELL_PIXMAP;
	  GTK_HELL_PIXMAP (hlist_row->cell[column])->pixmap = pixmap;
	  /* We set the mask even if it is NULL */
	  GTK_HELL_PIXMAP (hlist_row->cell[column])->mask = mask;
	}
      break;
    case GTK_HELL_PIXTEXT:
      if (text && pixmap)
	{
	  hlist_row->cell[column].type = GTK_HELL_PIXTEXT;
	  GTK_HELL_PIXTEXT (hlist_row->cell[column])->text = g_strdup (text);
	  GTK_HELL_PIXTEXT (hlist_row->cell[column])->spacing = spacing;
	  GTK_HELL_PIXTEXT (hlist_row->cell[column])->pixmap = pixmap;
	  GTK_HELL_PIXTEXT (hlist_row->cell[column])->mask = mask;
	}
      break;
    default:
      break;
    }

  if (hlist->column[column].auto_resize &&
      !GTK_HLIST_AUTO_RESIZE_BLOCKED(hlist))
    column_auto_resize (hlist, hlist_row, column, requisition.width);
}

static void
cell_size_request (GtkHList       *hlist,
		   GtkHListRow    *hlist_row,
		   gint            column,
		   GtkRequisition *requisition)
{
  GtkStyle *style;
  gint width;
  gint height;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));
  g_return_if_fail (requisition != NULL);

  get_cell_style (hlist, hlist_row, GTK_STATE_NORMAL, column, &style,
		  NULL, NULL);

  switch (hlist_row->cell[column].type)
    {
    case GTK_HELL_TEXT:
      requisition->width =
	gdk_string_width (style->font,
			  GTK_HELL_TEXT (hlist_row->cell[column])->text);
      requisition->height = style->font->ascent + style->font->descent;
      break;
    case GTK_HELL_PIXTEXT:
      gdk_window_get_size (GTK_HELL_PIXTEXT (hlist_row->cell[column])->pixmap,
			   &width, &height);
      requisition->width = width +
	GTK_HELL_PIXTEXT (hlist_row->cell[column])->spacing +
	gdk_string_width (style->font,
			  GTK_HELL_TEXT (hlist_row->cell[column])->text);

      requisition->height = MAX (style->font->ascent + style->font->descent,
				 height);
      break;
    case GTK_HELL_PIXMAP:
      gdk_window_get_size (GTK_HELL_PIXMAP (hlist_row->cell[column])->pixmap,
			   &width, &height);
      requisition->width = width;
      requisition->height = height;
      break;
    default:
      requisition->width  = 0;
      requisition->height = 0;
      break;
    }

  requisition->width  += hlist_row->cell[column].horizontal;
  requisition->height += hlist_row->cell[column].vertical;
}

/* PUBLIC INSERT/REMOVE ROW FUNCTIONS
 *   gtk_hlist_prepend
 *   gtk_hlist_append
 *   gtk_hlist_insert
 *   gtk_hlist_remove
 *   gtk_hlist_clear
 */
gint
gtk_hlist_prepend (GtkHList    *hlist,
		   gchar       *text[])
{
  g_return_val_if_fail (hlist != NULL, -1);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), -1);
  g_return_val_if_fail (text != NULL, -1);

  return GTK_HLIST_CLASS_FW (hlist)->insert_row (hlist, 0, text);
}

gint
gtk_hlist_append (GtkHList    *hlist,
		  gchar       *text[])
{
  g_return_val_if_fail (hlist != NULL, -1);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), -1);
  g_return_val_if_fail (text != NULL, -1);

  return GTK_HLIST_CLASS_FW (hlist)->insert_row (hlist, hlist->rows, text);
}

gint
gtk_hlist_insert (GtkHList    *hlist,
		  gint         row,
		  gchar       *text[])
{
  g_return_val_if_fail (hlist != NULL, -1);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), -1);
  g_return_val_if_fail (text != NULL, -1);

  if (row < 0 || row > hlist->rows)
    row = hlist->rows;

  return GTK_HLIST_CLASS_FW (hlist)->insert_row (hlist, row, text);
}

void
gtk_hlist_remove (GtkHList *hlist,
		  gint      row)
{
  GTK_HLIST_CLASS_FW (hlist)->remove_row (hlist, row);
}

void
gtk_hlist_clear (GtkHList *hlist)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));
  
  GTK_HLIST_CLASS_FW (hlist)->clear (hlist);
}

/* PRIVATE INSERT/REMOVE ROW FUNCTIONS
 *   real_insert_row
 *   real_remove_row
 *   real_clear
 *   real_row_move
 */
static gint
real_insert_row (GtkHList *hlist,
		 gint      row,
		 gchar    *text[])
{
  gint i;
  GtkHListRow *hlist_row;

  g_return_val_if_fail (hlist != NULL, -1);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), -1);
  g_return_val_if_fail (text != NULL, -1);

  /* return if out of bounds */
  if (row < 0 || row > hlist->rows)
    return -1;

  /* create the row */
  hlist_row = row_new (hlist);

  /* set the text in the row's columns */
  for (i = 0; i < hlist->columns; i++)
    if (text[i])
      GTK_HLIST_CLASS_FW (hlist)->set_cell_contents
	(hlist, hlist_row, i, GTK_HELL_TEXT, text[i], 0, NULL ,NULL);

  if (!hlist->rows)
    {
      hlist->row_list = g_list_append (hlist->row_list, hlist_row);
      hlist->row_list_end = hlist->row_list;
    }
  else
    {
      if (GTK_HLIST_AUTO_SORT(hlist))   /* override insertion pos */
	{
	  GList *work;
	  
	  row = 0;
	  work = hlist->row_list;
	  
	  if (hlist->sort_type == GTK_SORT_ASCENDING)
	    {
	      while (row < hlist->rows &&
		     hlist->compare (hlist, hlist_row,
				     GTK_HLIST_ROW (work)) > 0)
		{
		  row++;
		  work = work->next;
		}
	    }
	  else
	    {
	      while (row < hlist->rows &&
		     hlist->compare (hlist, hlist_row,
				     GTK_HLIST_ROW (work)) < 0)
		{
		  row++;
		  work = work->next;
		}
	    }
	}
      
      /* reset the row end pointer if we're inserting at the end of the list */
      if (row == hlist->rows)
	hlist->row_list_end = (g_list_append (hlist->row_list_end,
					      hlist_row))->next;
      else
	hlist->row_list = g_list_insert (hlist->row_list, hlist_row, row);

    }
  hlist->rows++;

  if ((hlist->rows % ROW_HEIGHT_BLOCKSIZE) == 0) {
    gint i;
    hlist->row_height = g_realloc(hlist->row_height, sizeof(guint)*(hlist->rows+ROW_HEIGHT_BLOCKSIZE));
    for (i = hlist->rows; i < hlist->rows+ROW_HEIGHT_BLOCKSIZE; i++)
      hlist->row_height[i] = 0;
  }

  hlist->row_height[row] = hlist->row_height[0];
  if (row < ROW_FROM_YPIXEL (hlist, 0))
    hlist->voffset -= (hlist->row_height[row] + HELL_SPACING);

  /* syncronize the selection list */
  sync_selection (hlist, row, SYNC_INSERT);

  if (hlist->rows == 1)
    {
      hlist->focus_row = 0;
      if (hlist->selection_mode == GTK_SELECTION_BROWSE)
	gtk_hlist_select_row (hlist, 0, -1);
    }

  hlist_row->draw_me = 1;
  /* redraw the list if it isn't frozen */
  if (HLIST_UNFROZEN (hlist))
    {
      adjust_adjustments (hlist, FALSE);

      if (gtk_hlist_row_is_visible (hlist, row) != GTK_VISIBILITY_NONE)
	draw_rows (hlist, NULL);
    }

  return row;
}

static void
real_remove_row (GtkHList *hlist,
		 gint      row)
{
  gint was_visible, was_selected;
  GList *list;
  GtkHListRow *hlist_row;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  /* return if out of bounds */
  if (row < 0 || row > (hlist->rows - 1))
    return;

  was_visible = (gtk_hlist_row_is_visible (hlist, row) != GTK_VISIBILITY_NONE);
  was_selected = 0;

  /* get the row we're going to delete */
  list = g_list_nth (hlist->row_list, row);
  g_assert (list != NULL);
  hlist_row = list->data;

  /* if we're removing a selected row, we have to make sure
   * it's properly unselected, and then sync up the hlist->selected
   * list to reflect the deincrimented indexies of rows after the
   * removal */
  if (hlist_row->state == GTK_STATE_SELECTED)
    gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[UNSELECT_ROW],
		     row, -1, NULL);

  /* reset the row end pointer if we're removing at the end of the list */
  hlist->rows--;
  if (hlist->row_list == list)
    hlist->row_list = g_list_next (list);
  if (hlist->row_list_end == list)
    hlist->row_list_end = g_list_previous (list);
  list = g_list_remove (list, hlist_row);

  /*if (hlist->focus_row >=0 &&
      (row <= hlist->focus_row || hlist->focus_row >= hlist->rows))
      hlist->focus_row--;*/

  if (row < ROW_FROM_YPIXEL (hlist, 0))
    hlist->voffset += hlist->row_height[row] + HELL_SPACING;

  sync_selection (hlist, row, SYNC_REMOVE);

  if (hlist->selection_mode == GTK_SELECTION_BROWSE && !hlist->selection &&
      hlist->focus_row >= 0)
    gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[SELECT_ROW],
		     hlist->focus_row, -1, NULL);

  /* toast the row */
  row_delete (hlist, hlist_row);

  /* need to draw all rows below this row */
  for (list = g_list_nth(hlist->row_list, row); list; list = list->next) {
    hlist_row = list->data;
    hlist_row->draw_me = 1;
  }

  /* redraw the row if it isn't frozen */
  if (HLIST_UNFROZEN (hlist))
    {
      adjust_adjustments (hlist, FALSE);

      if (was_visible)
	draw_rows (hlist, NULL);
    }
}

static void
real_clear (GtkHList *hlist)
{
  GList *list;
  GList *free_list;
  gint i;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  /* free up the selection list */
  g_list_free (hlist->selection);
  g_list_free (hlist->undo_selection);
  g_list_free (hlist->undo_unselection);

  hlist->selection = NULL;
  hlist->selection_end = NULL;
  hlist->undo_selection = NULL;
  hlist->undo_unselection = NULL;
  hlist->voffset = 0;
  hlist->focus_row = -1;
  hlist->anchor = -1;
  hlist->undo_anchor = -1;
  hlist->anchor_state = GTK_STATE_SELECTED;
  hlist->drag_pos = -1;

  /* remove all the rows */
  GTK_HLIST_SET_FLAG (hlist, HLIST_AUTO_RESIZE_BLOCKED);
  free_list = hlist->row_list;
  hlist->row_list = NULL;
  hlist->row_list_end = NULL;
  hlist->rows = 0;
  for (list = free_list; list; list = list->next)
    row_delete (hlist, GTK_HLIST_ROW (list));
  g_list_free (free_list);
  GTK_HLIST_UNSET_FLAG (hlist, HLIST_AUTO_RESIZE_BLOCKED);
  for (i = 0; i < hlist->columns; i++)
    if (hlist->column[i].auto_resize)
      {
	if (GTK_HLIST_SHOW_TITLES(hlist) && hlist->column[i].button)
	  gtk_hlist_set_column_width
	    (hlist, i, (hlist->column[i].button->requisition.width -
			(HELL_SPACING + (2 * COLUMN_INSET))));
	else
	  gtk_hlist_set_column_width (hlist, i, 0);
      }
  /* zero-out the scrollbars */
  if (hlist->vadjustment)
    {
      gtk_adjustment_set_value (hlist->vadjustment, 0.0);
      HLIST_REFRESH (hlist);
    }
  else
    gtk_widget_queue_resize (GTK_WIDGET (hlist));
}

static void
real_row_move (GtkHList *hlist,
	       gint      source_row,
	       gint      dest_row)
{
  GtkHListRow *hlist_row;
  GList *list;
  gint first, last;
  gint d;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (GTK_HLIST_AUTO_SORT(hlist))
    return;

  if (source_row < 0 || source_row >= hlist->rows ||
      dest_row   < 0 || dest_row   >= hlist->rows ||
      source_row == dest_row)
    return;

  gtk_hlist_freeze (hlist);

  /* unlink source row */
  hlist_row = g_list_nth_data (hlist->row_list, source_row);
  if (source_row == hlist->rows - 1)
    hlist->row_list_end = hlist->row_list_end->prev;
  hlist->row_list = g_list_remove (hlist->row_list, hlist_row);
  hlist->rows--;

  /* relink source row */
  hlist->row_list = g_list_insert (hlist->row_list, hlist_row, dest_row);
  if (dest_row == hlist->rows)
    hlist->row_list_end = hlist->row_list_end->next;
  hlist->rows++;

  /* sync selection */
  if (source_row > dest_row)
    {
      first = dest_row;
      last  = source_row;
      d = 1;
    }
  else
    {
      first = source_row;
      last  = dest_row;
      d = -1;
    }

  for (list = hlist->selection; list; list = list->next)
    {
      if (list->data == GINT_TO_POINTER (source_row))
	list->data = GINT_TO_POINTER (dest_row);
      else if (first <= GPOINTER_TO_INT (list->data) &&
	       last >= GPOINTER_TO_INT (list->data))
	list->data = GINT_TO_POINTER (GPOINTER_TO_INT (list->data) + d);
    }
  
  if (hlist->focus_row == source_row)
    hlist->focus_row = dest_row;
  else if (hlist->focus_row > first)
    hlist->focus_row += d;

  gtk_hlist_thaw (hlist);
}

/* PUBLIC ROW FUNCTIONS
 *   gtk_hlist_moveto
 *   gtk_hlist_set_row_height
 *   gtk_hlist_set_row_data
 *   gtk_hlist_set_row_data_full
 *   gtk_hlist_get_row_data
 *   gtk_hlist_find_row_from_data
 *   gtk_hlist_swap_rows
 *   gtk_hlist_row_move
 *   gtk_hlist_row_is_visible
 *   gtk_hlist_set_foreground
 *   gtk_hlist_set_background
 */
void
gtk_hlist_moveto (GtkHList *hlist,
		  gint      row,
		  gint      column,
		  gfloat    row_align,
		  gfloat    col_align)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (row < -1 || row >= hlist->rows)
    return;
  if (column < -1 || column >= hlist->columns)
    return;

  row_align = CLAMP (row_align, 0, 1);
  col_align = CLAMP (col_align, 0, 1);

  /* adjust horizontal scrollbar */
  if (hlist->hadjustment && column >= 0)
    {
      gint x;

      x = (COLUMN_LEFT (hlist, column) - HELL_SPACING - COLUMN_INSET -
	   (col_align * (hlist->hlist_window_width - 2 * COLUMN_INSET -
			 HELL_SPACING - hlist->column[column].area.width)));
      if (x < 0)
	gtk_adjustment_set_value (hlist->hadjustment, 0.0);
      else if (x > LIST_WIDTH (hlist) - hlist->hlist_window_width)
	gtk_adjustment_set_value 
	  (hlist->hadjustment, LIST_WIDTH (hlist) - hlist->hlist_window_width);
      else
	gtk_adjustment_set_value (hlist->hadjustment, x);
    }

  /* adjust vertical scrollbar */
  if (hlist->vadjustment && row >= 0)
    move_vertical (hlist, row, row_align);
}

void
gtk_hlist_set_row_height_row (GtkHList *hlist, gint row, guint height)
{
	hlist->row_height[row] = height;
}

static void
real_set_row_height (GtkHList *hlist, guint height)
{
	gint i;

	/* row_height[0] is the default height */
	hlist->row_height[0] = height;
	for (i = 0; i < hlist->rows; i++)
		hlist->row_height[i] = height;
}

void
gtk_hlist_set_row_height (GtkHList *hlist,
			  guint     height)
{
  GtkWidget *widget;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  widget = GTK_WIDGET (hlist);

  if (height > 0)
    {
      real_set_row_height(hlist, height);
      GTK_HLIST_SET_FLAG (hlist, HLIST_ROW_HEIGHT_SET);
    }
  else
    {
      GTK_HLIST_UNSET_FLAG (hlist, HLIST_ROW_HEIGHT_SET);
      real_set_row_height(hlist, height);
    }

  if (GTK_WIDGET_REALIZED (hlist))
    {
      if (!GTK_HLIST_ROW_HEIGHT_SET(hlist))
	{
	  hlist->row_height[0] = (widget->style->font->ascent +
			       widget->style->font->descent + 1);
	  real_set_row_height(hlist, hlist->row_height[0]);
	  hlist->row_center_offset = widget->style->font->ascent + 1.5;
	}
      else
	hlist->row_center_offset = 1.5 + (hlist->row_height[0] +
					  widget->style->font->ascent -
					  widget->style->font->descent - 1) / 2;
    }
      
  HLIST_REFRESH (hlist);
}

void
gtk_hlist_set_row_data (GtkHList *hlist,
			gint      row,
			gpointer  data)
{
  gtk_hlist_set_row_data_full (hlist, row, data, NULL);
}

void
gtk_hlist_set_row_data_full (GtkHList         *hlist,
			     gint              row,
			     gpointer          data,
			     GtkDestroyNotify  destroy)
{
  GtkHListRow *hlist_row;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (row < 0 || row > (hlist->rows - 1))
    return;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;

  if (hlist_row->destroy)
    hlist_row->destroy (hlist_row->data);
  
  hlist_row->data = data;
  hlist_row->destroy = destroy;
}

gpointer
gtk_hlist_get_row_data (GtkHList *hlist,
			gint      row)
{
  GtkHListRow *hlist_row;

  g_return_val_if_fail (hlist != NULL, NULL);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), NULL);

  if (row < 0 || row > (hlist->rows - 1))
    return NULL;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;
  return hlist_row->data;
}

gint
gtk_hlist_find_row_from_data (GtkHList *hlist,
			      gpointer  data)
{
  GList *list;
  gint n;

  g_return_val_if_fail (hlist != NULL, -1);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), -1);

  for (n = 0, list = hlist->row_list; list; n++, list = list->next)
    if (GTK_HLIST_ROW (list)->data == data)
      return n;

  return -1;
}

void 
gtk_hlist_swap_rows (GtkHList *hlist,
		     gint      row1, 
		     gint      row2)
{
  gint first, last;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));
  g_return_if_fail (row1 != row2);

  if (GTK_HLIST_AUTO_SORT(hlist))
    return;

  gtk_hlist_freeze (hlist);

  first = MIN (row1, row2);
  last  = MAX (row1, row2);

  gtk_hlist_row_move (hlist, last, first);
  gtk_hlist_row_move (hlist, first + 1, last);
  
  gtk_hlist_thaw (hlist);
}

void
gtk_hlist_row_move (GtkHList *hlist,
		    gint      source_row,
		    gint      dest_row)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (GTK_HLIST_AUTO_SORT(hlist))
    return;

  if (source_row < 0 || source_row >= hlist->rows ||
      dest_row   < 0 || dest_row   >= hlist->rows ||
      source_row == dest_row)
    return;

  gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[ROW_MOVE],
		   source_row, dest_row);
}

GtkVisibility
gtk_hlist_row_is_visible (GtkHList *hlist,
			  gint      row)
{
  gint top;

  g_return_val_if_fail (hlist != NULL, 0);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), 0);

  if (row < 0 || row >= hlist->rows)
    return GTK_VISIBILITY_NONE;

  if (hlist->row_height[row] == 0)
    return GTK_VISIBILITY_NONE;

  if (row < ROW_FROM_YPIXEL (hlist, 0))
    return GTK_VISIBILITY_NONE;

  if (row > ROW_FROM_YPIXEL (hlist, hlist->hlist_window_height))
    return GTK_VISIBILITY_NONE;

  top = ROW_TOP_YPIXEL (hlist, row);

  if ((top < 0)
      || ((top + hlist->row_height[row]) >= hlist->hlist_window_height))
    return GTK_VISIBILITY_PARTIAL;

  return GTK_VISIBILITY_FULL;
}

void
gtk_hlist_set_foreground (GtkHList *hlist,
			  gint      row,
			  GdkColor *color)
{
  GtkHListRow *hlist_row;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (row < 0 || row >= hlist->rows)
    return;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;

  if (color)
    {
      hlist_row->foreground = *color;
      hlist_row->fg_set = TRUE;
      if (GTK_WIDGET_REALIZED (hlist))
	gdk_color_alloc (gtk_widget_get_colormap (GTK_WIDGET (hlist)),
			 &hlist_row->foreground);
    }
  else
    hlist_row->fg_set = FALSE;

  if (HLIST_UNFROZEN (hlist) && gtk_hlist_row_is_visible (hlist, row) != GTK_VISIBILITY_NONE)
    GTK_HLIST_CLASS_FW (hlist)->draw_row (hlist, NULL, row, hlist_row);
}

void
gtk_hlist_set_background (GtkHList *hlist,
			  gint      row,
			  GdkColor *color)
{
  GtkHListRow *hlist_row;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (row < 0 || row >= hlist->rows)
    return;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;

  if (color)
    {
      hlist_row->background = *color;
      hlist_row->bg_set = TRUE;
      if (GTK_WIDGET_REALIZED (hlist))
	gdk_color_alloc (gtk_widget_get_colormap (GTK_WIDGET (hlist)),
			 &hlist_row->background);
    }
  else
    hlist_row->bg_set = FALSE;

  if (HLIST_UNFROZEN (hlist)
      && (gtk_hlist_row_is_visible (hlist, row) != GTK_VISIBILITY_NONE))
    GTK_HLIST_CLASS_FW (hlist)->draw_row (hlist, NULL, row, hlist_row);
}

/* PUBLIC ROW/HELL STYLE FUNCTIONS
 *   gtk_hlist_set_cell_style
 *   gtk_hlist_get_cell_style
 *   gtk_hlist_set_row_style
 *   gtk_hlist_get_row_style
 */
void
gtk_hlist_set_cell_style (GtkHList *hlist,
			  gint      row,
			  gint      column,
			  GtkStyle *style)
{
  GtkRequisition requisition = { 0, 0 };
  GtkHListRow *hlist_row;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (row < 0 || row >= hlist->rows)
    return;
  if (column < 0 || column >= hlist->columns)
    return;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;

  if (hlist_row->cell[column].style == style)
    return;

  if (hlist->column[column].auto_resize &&
      !GTK_HLIST_AUTO_RESIZE_BLOCKED(hlist))
    GTK_HLIST_CLASS_FW (hlist)->cell_size_request (hlist, hlist_row,
						   column, &requisition);

  if (hlist_row->cell[column].style)
    {
      if (GTK_WIDGET_REALIZED (hlist))
        gtk_style_detach (hlist_row->cell[column].style);
      gtk_style_unref (hlist_row->cell[column].style);
    }

  hlist_row->cell[column].style = style;

  if (hlist_row->cell[column].style)
    {
      gtk_style_ref (hlist_row->cell[column].style);
      
      if (GTK_WIDGET_REALIZED (hlist))
        hlist_row->cell[column].style =
	  gtk_style_attach (hlist_row->cell[column].style,
			    hlist->hlist_window);
    }

  column_auto_resize (hlist, hlist_row, column, requisition.width);

  /* redraw the list if it's not frozen */
  if (HLIST_UNFROZEN (hlist))
    {
      if (gtk_hlist_row_is_visible (hlist, row) != GTK_VISIBILITY_NONE)
	GTK_HLIST_CLASS_FW (hlist)->draw_row (hlist, NULL, row, hlist_row);
    }
}

GtkStyle *
gtk_hlist_get_cell_style (GtkHList *hlist,
			  gint      row,
			  gint      column)
{
  GtkHListRow *hlist_row;

  g_return_val_if_fail (hlist != NULL, NULL);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), NULL);

  if (row < 0 || row >= hlist->rows || column < 0 || column >= hlist->columns)
    return NULL;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;

  return hlist_row->cell[column].style;
}

void
gtk_hlist_set_row_style (GtkHList *hlist,
			 gint      row,
			 GtkStyle *style)
{
  GtkRequisition requisition;
  GtkHListRow *hlist_row;
  gint *old_width;
  gint i;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (row < 0 || row >= hlist->rows)
    return;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;

  if (hlist_row->style == style)
    return;

  old_width = g_new (gint, hlist->columns);

  if (!GTK_HLIST_AUTO_RESIZE_BLOCKED(hlist))
    {
      for (i = 0; i < hlist->columns; i++)
	if (hlist->column[i].auto_resize)
	  {
	    GTK_HLIST_CLASS_FW (hlist)->cell_size_request (hlist, hlist_row,
							   i, &requisition);
	    old_width[i] = requisition.width;
	  }
    }

  if (hlist_row->style)
    {
      if (GTK_WIDGET_REALIZED (hlist))
        gtk_style_detach (hlist_row->style);
      gtk_style_unref (hlist_row->style);
    }

  hlist_row->style = style;

  if (hlist_row->style)
    {
      gtk_style_ref (hlist_row->style);
      
      if (GTK_WIDGET_REALIZED (hlist))
        hlist_row->style = gtk_style_attach (hlist_row->style,
					     hlist->hlist_window);
    }

  if (GTK_HLIST_AUTO_RESIZE_BLOCKED(hlist))
    for (i = 0; i < hlist->columns; i++)
      column_auto_resize (hlist, hlist_row, i, old_width[i]);

  g_free (old_width);

  /* redraw the list if it's not frozen */
  if (HLIST_UNFROZEN (hlist))
    {
      if (gtk_hlist_row_is_visible (hlist, row) != GTK_VISIBILITY_NONE)
	GTK_HLIST_CLASS_FW (hlist)->draw_row (hlist, NULL, row, hlist_row);
    }
}

GtkStyle *
gtk_hlist_get_row_style (GtkHList *hlist,
			 gint      row)
{
  GtkHListRow *hlist_row;

  g_return_val_if_fail (hlist != NULL, NULL);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), NULL);

  if (row < 0 || row >= hlist->rows)
    return NULL;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;

  return hlist_row->style;
}

/* PUBLIC SELECTION FUNCTIONS
 *   gtk_hlist_set_selectable
 *   gtk_hlist_get_selectable
 *   gtk_hlist_select_row
 *   gtk_hlist_unselect_row
 *   gtk_hlist_select_all
 *   gtk_hlist_unselect_all
 *   gtk_hlist_undo_selection
 */
void
gtk_hlist_set_selectable (GtkHList *hlist,
			  gint      row,
			  gboolean  selectable)
{
  GtkHListRow *hlist_row;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (row < 0 || row >= hlist->rows)
    return;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;

  if (selectable == hlist_row->selectable)
    return;

  hlist_row->selectable = selectable;

  if (!selectable && hlist_row->state == GTK_STATE_SELECTED)
    {
      if (hlist->anchor >= 0 &&
	  hlist->selection_mode == GTK_SELECTION_EXTENDED)
	{
	  hlist->drag_button = 0;
	  remove_grab (hlist);
	  GTK_HLIST_CLASS_FW (hlist)->resync_selection (hlist, NULL);
	}
      gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[UNSELECT_ROW],
		       row, -1, NULL);
    }      
}

gboolean
gtk_hlist_get_selectable (GtkHList *hlist,
			  gint      row)
{
  g_return_val_if_fail (hlist != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), FALSE);

  if (row < 0 || row >= hlist->rows)
    return FALSE;

  return GTK_HLIST_ROW (g_list_nth (hlist->row_list, row))->selectable;
}

void
gtk_hlist_select_row (GtkHList *hlist,
		      gint      row,
		      gint      column)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (row < 0 || row >= hlist->rows)
    return;
  if (column < -1 || column >= hlist->columns)
    return;

  gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[SELECT_ROW],
		   row, column, NULL);
}

void
gtk_hlist_unselect_row (GtkHList *hlist,
			gint      row,
			gint      column)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (row < 0 || row >= hlist->rows)
    return;
  if (column < -1 || column >= hlist->columns)
    return;

  gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[UNSELECT_ROW],
		   row, column, NULL);
}

void
gtk_hlist_select_all (GtkHList *hlist)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  GTK_HLIST_CLASS_FW (hlist)->select_all (hlist);
}

void
gtk_hlist_unselect_all (GtkHList *hlist)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  GTK_HLIST_CLASS_FW (hlist)->unselect_all (hlist);
}

void
gtk_hlist_undo_selection (GtkHList *hlist)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (hlist->selection_mode == GTK_SELECTION_EXTENDED &&
      (hlist->undo_selection || hlist->undo_unselection))
    gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[UNDO_SELECTION]);
}

/* PRIVATE SELECTION FUNCTIONS
 *   selection_find
 *   toggle_row
 *   fake_toggle_row
 *   toggle_focus_row
 *   toggle_add_mode
 *   real_select_row
 *   real_unselect_row
 *   real_select_all
 *   real_unselect_all
 *   fake_unselect_all
 *   real_undo_selection
 *   set_anchor
 *   resync_selection
 *   update_extended_selection
 *   start_selection
 *   end_selection
 *   extend_selection
 *   sync_selection
 */
static GList *
selection_find (GtkHList *hlist,
		gint      row_number,
		GList    *row_list_element)
{
  return g_list_find (hlist->selection, GINT_TO_POINTER (row_number));
}

static void
toggle_row (GtkHList *hlist,
	    gint      row,
	    gint      column,
	    GdkEvent *event)
{
  GtkHListRow *hlist_row;

  switch (hlist->selection_mode)
    {
    case GTK_SELECTION_EXTENDED:
    case GTK_SELECTION_MULTIPLE:
    case GTK_SELECTION_SINGLE:
      hlist_row = g_list_nth (hlist->row_list, row)->data;

      if (!hlist_row)
	return;

      if (hlist_row->state == GTK_STATE_SELECTED)
	{
	  gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[UNSELECT_ROW],
			   row, column, event);
	  return;
	}
    case GTK_SELECTION_BROWSE:
      gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[SELECT_ROW],
		       row, column, event);
      break;
    }
}

static void
fake_toggle_row (GtkHList *hlist,
		 gint      row)
{
  GList *work;

  work = g_list_nth (hlist->row_list, row);

  if (!work || !GTK_HLIST_ROW (work)->selectable)
    return;
  
  if (GTK_HLIST_ROW (work)->state == GTK_STATE_NORMAL)
    hlist->anchor_state = GTK_HLIST_ROW (work)->state = GTK_STATE_SELECTED;
  else
    hlist->anchor_state = GTK_HLIST_ROW (work)->state = GTK_STATE_NORMAL;
  
  if (HLIST_UNFROZEN (hlist) &&
      gtk_hlist_row_is_visible (hlist, row) != GTK_VISIBILITY_NONE)
    GTK_HLIST_CLASS_FW (hlist)->draw_row (hlist, NULL, row,
					  GTK_HLIST_ROW (work));
}

static void
toggle_focus_row (GtkHList *hlist)
{
  g_return_if_fail (hlist != 0);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if ((gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (hlist)) ||
      hlist->focus_row < 0 || hlist->focus_row >= hlist->rows)
    return;

  switch (hlist->selection_mode)
    {
    case  GTK_SELECTION_SINGLE:
    case  GTK_SELECTION_MULTIPLE:
      toggle_row (hlist, hlist->focus_row, 0, NULL);
      break;
    case GTK_SELECTION_EXTENDED:
      g_list_free (hlist->undo_selection);
      g_list_free (hlist->undo_unselection);
      hlist->undo_selection = NULL;
      hlist->undo_unselection = NULL;

      hlist->anchor = hlist->focus_row;
      hlist->drag_pos = hlist->focus_row;
      hlist->undo_anchor = hlist->focus_row;
      
      if (GTK_HLIST_ADD_MODE(hlist))
	fake_toggle_row (hlist, hlist->focus_row);
      else
	GTK_HLIST_CLASS_FW (hlist)->fake_unselect_all (hlist,hlist->focus_row);

      GTK_HLIST_CLASS_FW (hlist)->resync_selection (hlist, NULL);
      break;
    default:
      break;
    }
}

static void
toggle_add_mode (GtkHList *hlist)
{
  g_return_if_fail (hlist != 0);
  g_return_if_fail (GTK_IS_HLIST (hlist));
  
  if ((gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (hlist)) ||
      hlist->selection_mode != GTK_SELECTION_EXTENDED)
    return;

  gtk_hlist_draw_focus (GTK_WIDGET (hlist));
  if (!GTK_HLIST_ADD_MODE(hlist))
    {
      GTK_HLIST_SET_FLAG (hlist, HLIST_ADD_MODE);
      gdk_gc_set_line_attributes (hlist->xor_gc, 1,
				  GDK_LINE_ON_OFF_DASH, 0, 0);
      gdk_gc_set_dashes (hlist->xor_gc, 0, "\4\4", 2);
    }
  else
    {
      GTK_HLIST_UNSET_FLAG (hlist, HLIST_ADD_MODE);
      gdk_gc_set_line_attributes (hlist->xor_gc, 1, GDK_LINE_SOLID, 0, 0);
      hlist->anchor_state = GTK_STATE_SELECTED;
    }
  gtk_hlist_draw_focus (GTK_WIDGET (hlist));
}

static void
real_select_row (GtkHList *hlist,
		 gint      row,
		 gint      column,
		 GdkEvent *event)
{
  GtkHListRow *hlist_row;
  GList *list;
  gint sel_row;
  gboolean row_selected;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (row < 0 || row > (hlist->rows - 1))
    return;

  switch (hlist->selection_mode)
    {
    case GTK_SELECTION_SINGLE:
    case GTK_SELECTION_BROWSE:

      row_selected = FALSE;
      list = hlist->selection;

      while (list)
	{
	  sel_row = GPOINTER_TO_INT (list->data);
	  list = list->next;

	  if (row == sel_row)
	    row_selected = TRUE;
	  else
	    gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[UNSELECT_ROW], 
			     sel_row, column, event);
	}

      if (row_selected)
	return;
      
    default:
      break;
    }

  hlist_row = (g_list_nth (hlist->row_list, row))->data;

  if (hlist_row->state != GTK_STATE_NORMAL || !hlist_row->selectable)
    return;

  hlist_row->state = GTK_STATE_SELECTED;
  if (!hlist->selection)
    {
      hlist->selection = g_list_append (hlist->selection,
					GINT_TO_POINTER (row));
      hlist->selection_end = hlist->selection;
    }
  else
    hlist->selection_end = 
      g_list_append (hlist->selection_end, GINT_TO_POINTER (row))->next;
  
  if (HLIST_UNFROZEN (hlist)
      && (gtk_hlist_row_is_visible (hlist, row) != GTK_VISIBILITY_NONE))
    GTK_HLIST_CLASS_FW (hlist)->draw_row (hlist, NULL, row, hlist_row);
}

static void
real_unselect_row (GtkHList *hlist,
		   gint      row,
		   gint      column,
		   GdkEvent *event)
{
  GtkHListRow *hlist_row;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (row < 0 || row > (hlist->rows - 1))
    return;

  hlist_row = (g_list_nth (hlist->row_list, row))->data;

  if (hlist_row->state == GTK_STATE_SELECTED)
    {
      hlist_row->state = GTK_STATE_NORMAL;

      if (hlist->selection_end && 
	  hlist->selection_end->data == GINT_TO_POINTER (row))
	hlist->selection_end = hlist->selection_end->prev;

      hlist->selection = g_list_remove (hlist->selection,
					GINT_TO_POINTER (row));
      
      if (HLIST_UNFROZEN (hlist)
	  && (gtk_hlist_row_is_visible (hlist, row) != GTK_VISIBILITY_NONE))
	GTK_HLIST_CLASS_FW (hlist)->draw_row (hlist, NULL, row, hlist_row);
    }
}

static void
real_select_all (GtkHList *hlist)
{
  GList *list;
  gint i;
 
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (hlist))
    return;

  switch (hlist->selection_mode)
    {
    case GTK_SELECTION_SINGLE:
    case GTK_SELECTION_BROWSE:
      return;

    case GTK_SELECTION_EXTENDED:
      g_list_free (hlist->undo_selection);
      g_list_free (hlist->undo_unselection);
      hlist->undo_selection = NULL;
      hlist->undo_unselection = NULL;
	  
      if (hlist->rows &&
	  ((GtkHListRow *) (hlist->row_list->data))->state !=
	  GTK_STATE_SELECTED)
	fake_toggle_row (hlist, 0);

      hlist->anchor_state =  GTK_STATE_SELECTED;
      hlist->anchor = 0;
      hlist->drag_pos = 0;
      hlist->undo_anchor = hlist->focus_row;
      update_extended_selection (hlist, hlist->rows);
      GTK_HLIST_CLASS_FW (hlist)->resync_selection (hlist, NULL);
      return;

    case GTK_SELECTION_MULTIPLE:
      for (i = 0, list = hlist->row_list; list; i++, list = list->next)
	{
	  if (((GtkHListRow *)(list->data))->state == GTK_STATE_NORMAL)
	    gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[SELECT_ROW],
			     i, -1, NULL);
	}
      return;
    }
}

static void
real_unselect_all (GtkHList *hlist)
{
  GList *list;
  gint i;
 
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (hlist))
    return;

  switch (hlist->selection_mode)
    {
    case GTK_SELECTION_BROWSE:
      if (hlist->focus_row >= 0)
	{
	  gtk_signal_emit (GTK_OBJECT (hlist),
			   hlist_signals[SELECT_ROW],
			   hlist->focus_row, -1, NULL);
	  return;
	}
      break;
    case GTK_SELECTION_EXTENDED:
      g_list_free (hlist->undo_selection);
      g_list_free (hlist->undo_unselection);
      hlist->undo_selection = NULL;
      hlist->undo_unselection = NULL;

      hlist->anchor = -1;
      hlist->drag_pos = -1;
      hlist->undo_anchor = hlist->focus_row;
      break;
    default:
      break;
    }

  list = hlist->selection;
  while (list)
    {
      i = GPOINTER_TO_INT (list->data);
      list = list->next;
      gtk_signal_emit (GTK_OBJECT (hlist),
		       hlist_signals[UNSELECT_ROW], i, -1, NULL);
    }
}

static void
fake_unselect_all (GtkHList *hlist,
		   gint      row)
{
  GList *list;
  GList *work;
  gint i;

  if (row >= 0 && (work = g_list_nth (hlist->row_list, row)))
    {
      if (GTK_HLIST_ROW (work)->state == GTK_STATE_NORMAL &&
	  GTK_HLIST_ROW (work)->selectable)
	{
	  GTK_HLIST_ROW (work)->state = GTK_STATE_SELECTED;
	  
	  if (HLIST_UNFROZEN (hlist) &&
	      gtk_hlist_row_is_visible (hlist, row) != GTK_VISIBILITY_NONE)
	    GTK_HLIST_CLASS_FW (hlist)->draw_row (hlist, NULL, row,
						  GTK_HLIST_ROW (work));
	}  
    }

  hlist->undo_selection = hlist->selection;
  hlist->selection = NULL;
  hlist->selection_end = NULL;
  
  for (list = hlist->undo_selection; list; list = list->next)
    {
      if ((i = GPOINTER_TO_INT (list->data)) == row ||
	  !(work = g_list_nth (hlist->row_list, i)))
	continue;

      GTK_HLIST_ROW (work)->state = GTK_STATE_NORMAL;
      if (HLIST_UNFROZEN (hlist) &&
	  gtk_hlist_row_is_visible (hlist, i) != GTK_VISIBILITY_NONE)
	GTK_HLIST_CLASS_FW (hlist)->draw_row (hlist, NULL, i,
					      GTK_HLIST_ROW (work));
    }
}

static void
real_undo_selection (GtkHList *hlist)
{
  GList *work;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if ((gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (hlist)) ||
      hlist->selection_mode != GTK_SELECTION_EXTENDED)
    return;

  GTK_HLIST_CLASS_FW (hlist)->resync_selection (hlist, NULL);

  if (!(hlist->undo_selection || hlist->undo_unselection))
    {
      gtk_hlist_unselect_all (hlist);
      return;
    }

  for (work = hlist->undo_selection; work; work = work->next)
    gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[SELECT_ROW],
		     GPOINTER_TO_INT (work->data), -1, NULL);

  for (work = hlist->undo_unselection; work; work = work->next)
    {
      /* g_print ("unselect %d\n",GPOINTER_TO_INT (work->data)); */
      gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[UNSELECT_ROW], 
		       GPOINTER_TO_INT (work->data), -1, NULL);
    }

  if (GTK_WIDGET_HAS_FOCUS(hlist) && hlist->focus_row != hlist->undo_anchor)
    {
      gtk_hlist_draw_focus (GTK_WIDGET (hlist));
      hlist->focus_row = hlist->undo_anchor;
      gtk_hlist_draw_focus (GTK_WIDGET (hlist));
    }
  else
    hlist->focus_row = hlist->undo_anchor;
  
  hlist->undo_anchor = -1;
 
  g_list_free (hlist->undo_selection);
  g_list_free (hlist->undo_unselection);
  hlist->undo_selection = NULL;
  hlist->undo_unselection = NULL;

  if (ROW_TOP_YPIXEL (hlist, hlist->focus_row) + hlist->row_height[hlist->focus_row] >
      hlist->hlist_window_height)
    gtk_hlist_moveto (hlist, hlist->focus_row, -1, 1, 0);
  else if (ROW_TOP_YPIXEL (hlist, hlist->focus_row) < 0)
    gtk_hlist_moveto (hlist, hlist->focus_row, -1, 0, 0);
}

static void
set_anchor (GtkHList *hlist,
	    gboolean  add_mode,
	    gint      anchor,
	    gint      undo_anchor)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));
  
  if (hlist->selection_mode != GTK_SELECTION_EXTENDED || hlist->anchor >= 0)
    return;

  g_list_free (hlist->undo_selection);
  g_list_free (hlist->undo_unselection);
  hlist->undo_selection = NULL;
  hlist->undo_unselection = NULL;

  if (add_mode)
    fake_toggle_row (hlist, anchor);
  else
    {
      GTK_HLIST_CLASS_FW (hlist)->fake_unselect_all (hlist, anchor);
      hlist->anchor_state = GTK_STATE_SELECTED;
    }

  hlist->anchor = anchor;
  hlist->drag_pos = anchor;
  hlist->undo_anchor = undo_anchor;
}

static void
resync_selection (GtkHList *hlist,
		  GdkEvent *event)
{
  gint i;
  gint e;
  gint row;
  GList *list, *nth;
  GtkHListRow *hlist_row;

  if (hlist->selection_mode != GTK_SELECTION_EXTENDED)
    return;

  if (hlist->anchor < 0 || hlist->drag_pos < 0)
    return;

  gtk_hlist_freeze (hlist);

  i = MIN (hlist->anchor, hlist->drag_pos);
  e = MAX (hlist->anchor, hlist->drag_pos);

  if (hlist->undo_selection)
    {
      list = hlist->selection;
      hlist->selection = hlist->undo_selection;
      hlist->selection_end = g_list_last (hlist->selection);
      hlist->undo_selection = list;
      list = hlist->selection;
      while (list)
	{
	  row = GPOINTER_TO_INT (list->data);
	  list = list->next;
	  if (row < i || row > e)
	    {
	      nth = g_list_nth (hlist->row_list, row);
	      if (nth)
		hlist_row = nth->data;
	      else
		continue;
	      if (hlist_row->selectable)
		{
		  hlist_row->draw_me = 1;
		  hlist_row->state = GTK_STATE_SELECTED;
		  gtk_signal_emit (GTK_OBJECT (hlist),
				   hlist_signals[UNSELECT_ROW],
				   row, -1, event);
		  hlist->undo_selection = g_list_prepend
		    (hlist->undo_selection, GINT_TO_POINTER (row));
		}
	    }
	}
    }    

  if (hlist->anchor < hlist->drag_pos)
    {
      for (list = g_list_nth (hlist->row_list, i); i <= e;
	   i++, list = list->next) {
	if (!list)
	  break;
	if (GTK_HLIST_ROW (list)->selectable)
	  {
	    if (g_list_find (hlist->selection, GINT_TO_POINTER(i)))
	      {
		if (GTK_HLIST_ROW (list)->state == GTK_STATE_NORMAL)
		  {
		    GTK_HLIST_ROW(list)->draw_me = 1;
		    GTK_HLIST_ROW (list)->state = GTK_STATE_SELECTED;
		    gtk_signal_emit (GTK_OBJECT (hlist),
				     hlist_signals[UNSELECT_ROW],
				     i, -1, event);
		    hlist->undo_selection =
		      g_list_prepend (hlist->undo_selection,
				      GINT_TO_POINTER (i));
		  }
	      }
	    else if (GTK_HLIST_ROW (list)->state == GTK_STATE_SELECTED)
	      {
		GTK_HLIST_ROW (list)->state = GTK_STATE_NORMAL;
		hlist->undo_unselection =
		  g_list_prepend (hlist->undo_unselection,
				  GINT_TO_POINTER (i));
	      }
	  }
      }
    }
  else
    {
      for (list = g_list_nth (hlist->row_list, e); i <= e;
	   e--, list = list->prev) {
	if (!list)
	  break;
	if (GTK_HLIST_ROW (list)->selectable)
	  {
	    if (g_list_find (hlist->selection, GINT_TO_POINTER(e)))
	      {
		if (GTK_HLIST_ROW (list)->state == GTK_STATE_NORMAL)
		  {
		    GTK_HLIST_ROW(list)->draw_me = 1;
		    GTK_HLIST_ROW (list)->state = GTK_STATE_SELECTED;
		    gtk_signal_emit (GTK_OBJECT (hlist),
				     hlist_signals[UNSELECT_ROW],
				     e, -1, event);
		    hlist->undo_selection =
		      g_list_prepend (hlist->undo_selection,
				      GINT_TO_POINTER (e));
		  }
	      }
	    else if (GTK_HLIST_ROW (list)->state == GTK_STATE_SELECTED)
	      {
		GTK_HLIST_ROW (list)->state = GTK_STATE_NORMAL;
		hlist->undo_unselection =
		  g_list_prepend (hlist->undo_unselection,
				  GINT_TO_POINTER (e));
	      }
	  }
      }
    }
  
  hlist->undo_unselection = g_list_reverse (hlist->undo_unselection);
  for (list = hlist->undo_unselection; list; list = list->next)
    gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[SELECT_ROW],
		     GPOINTER_TO_INT (list->data), -1, event);

  hlist->anchor = -1;
  hlist->drag_pos = -1;

  gtk_hlist_thaw (hlist);
}

static void
update_extended_selection (GtkHList *hlist,
			   gint      row)
{
  gint i;
  GList *list;
  GdkRectangle area;
  gint s1 = -1;
  gint s2 = -1;
  gint e1 = -1;
  gint e2 = -1;
  gint y1 = hlist->hlist_window_height;
  gint y2 = hlist->hlist_window_height;
  gint h1 = 0;
  gint h2 = 0;
  gint top;

  if (hlist->selection_mode != GTK_SELECTION_EXTENDED || hlist->anchor == -1)
    return;

  if (row < 0)
    row = 0;
  if (row >= hlist->rows)
    row = hlist->rows - 1;

  /* extending downwards */
  if (row > hlist->drag_pos && hlist->anchor <= hlist->drag_pos)
    {
      s2 = hlist->drag_pos + 1;
      e2 = row;
    }
  /* extending upwards */
  else if (row < hlist->drag_pos && hlist->anchor >= hlist->drag_pos)
    {
      s2 = row;
      e2 = hlist->drag_pos - 1;
    }
  else if (row < hlist->drag_pos && hlist->anchor < hlist->drag_pos)
    {
      e1 = hlist->drag_pos;
      /* row and drag_pos on different sides of anchor :
	 take back the selection between anchor and drag_pos,
         select between anchor and row */
      if (row < hlist->anchor)
	{
	  s1 = hlist->anchor + 1;
	  s2 = row;
	  e2 = hlist->anchor - 1;
	}
      /* take back the selection between anchor and drag_pos */
      else
	s1 = row + 1;
    }
  else if (row > hlist->drag_pos && hlist->anchor > hlist->drag_pos)
    {
      s1 = hlist->drag_pos;
      /* row and drag_pos on different sides of anchor :
	 take back the selection between anchor and drag_pos,
         select between anchor and row */
      if (row > hlist->anchor)
	{
	  e1 = hlist->anchor - 1;
	  s2 = hlist->anchor + 1;
	  e2 = row;
	}
      /* take back the selection between anchor and drag_pos */
      else
	e1 = row - 1;
    }

  hlist->drag_pos = row;

  area.x = 0;
  area.width = hlist->hlist_window_width;

  /* restore the elements between s1 and e1 */
  if (s1 >= 0)
    {
      for (i = s1, list = g_list_nth (hlist->row_list, i); i <= e1;
	   i++, list = list->next)
	if (GTK_HLIST_ROW (list)->selectable)
	  {
	    if (GTK_HLIST_CLASS_FW (hlist)->selection_find (hlist, i, list))
	      GTK_HLIST_ROW (list)->state = GTK_STATE_SELECTED;
	    else
	      GTK_HLIST_ROW (list)->state = GTK_STATE_NORMAL;
	  }

      top = ROW_TOP_YPIXEL (hlist, hlist->focus_row);

      if (top + hlist->row_height[row] <= 0)
	{
	  area.y = 0;
	  area.height = ROW_TOP_YPIXEL (hlist, e1) + hlist->row_height[row];
	  draw_rows (hlist, &area);
	  gtk_hlist_moveto (hlist, hlist->focus_row, -1, 0, 0);
	}
      else if (top >= hlist->hlist_window_height)
	{
	  area.y = ROW_TOP_YPIXEL (hlist, s1) - 1;
	  area.height = hlist->hlist_window_height - area.y;
	  draw_rows (hlist, &area);
	  gtk_hlist_moveto (hlist, hlist->focus_row, -1, 1, 0);
	}
      else if (top < 0)
	gtk_hlist_moveto (hlist, hlist->focus_row, -1, 0, 0);
      else if (top + hlist->row_height[row] > hlist->hlist_window_height)
	gtk_hlist_moveto (hlist, hlist->focus_row, -1, 1, 0);

      y1 = ROW_TOP_YPIXEL (hlist, s1) - 1;
      h1 = (e1 - s1 + 1) * (hlist->row_height[row] + HELL_SPACING);
    }

  /* extend the selection between s2 and e2 */
  if (s2 >= 0)
    {
      for (i = s2, list = g_list_nth (hlist->row_list, i); i <= e2;
	   i++, list = list->next)
	if (GTK_HLIST_ROW (list)->selectable &&
	    GTK_HLIST_ROW (list)->state != hlist->anchor_state)
	  GTK_HLIST_ROW (list)->state = hlist->anchor_state;

      top = ROW_TOP_YPIXEL (hlist, hlist->focus_row);

      if (top + hlist->row_height[row] <= 0)
	{
	  area.y = 0;
	  area.height = ROW_TOP_YPIXEL (hlist, e2) + hlist->row_height[row];
	  draw_rows (hlist, &area);
	  gtk_hlist_moveto (hlist, hlist->focus_row, -1, 0, 0);
	}
      else if (top >= hlist->hlist_window_height)
	{
	  area.y = ROW_TOP_YPIXEL (hlist, s2) - 1;
	  area.height = hlist->hlist_window_height - area.y;
	  draw_rows (hlist, &area);
	  gtk_hlist_moveto (hlist, hlist->focus_row, -1, 1, 0);
	}
      else if (top < 0)
	gtk_hlist_moveto (hlist, hlist->focus_row, -1, 0, 0);
      else if (top + hlist->row_height[row] > hlist->hlist_window_height)
	gtk_hlist_moveto (hlist, hlist->focus_row, -1, 1, 0);

      y2 = ROW_TOP_YPIXEL (hlist, s2) - 1;
      h2 = (e2 - s2 + 1) * (hlist->row_height[row] + HELL_SPACING);
    }

  area.y = MAX (0, MIN (y1, y2));
  if (area.y > hlist->hlist_window_height)
    area.y = 0;
  area.height = MIN (hlist->hlist_window_height, h1 + h2);
  if (s1 >= 0 && s2 >= 0)
    area.height += (hlist->row_height[row] + HELL_SPACING);
  draw_rows (hlist, &area);
}

static void
start_selection (GtkHList *hlist)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (hlist))
    return;

  set_anchor (hlist, GTK_HLIST_ADD_MODE(hlist), hlist->focus_row,
	      hlist->focus_row);
}

static void
end_selection (GtkHList *hlist)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_FOCUS(hlist))
    return;

  GTK_HLIST_CLASS_FW (hlist)->resync_selection (hlist, NULL);
}

static void
extend_selection (GtkHList      *hlist,
		  GtkScrollType  scroll_type,
		  gfloat         position,
		  gboolean       auto_start_selection)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if ((gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (hlist)) ||
      hlist->selection_mode != GTK_SELECTION_EXTENDED)
    return;

  if (auto_start_selection)
    set_anchor (hlist, GTK_HLIST_ADD_MODE(hlist), hlist->focus_row,
		hlist->focus_row);
  else if (hlist->anchor == -1)
    return;

  move_focus_row (hlist, scroll_type, position);

  if (ROW_TOP_YPIXEL (hlist, hlist->focus_row) + hlist->row_height[hlist->focus_row] >
      hlist->hlist_window_height)
    gtk_hlist_moveto (hlist, hlist->focus_row, -1, 1, 0);
  else if (ROW_TOP_YPIXEL (hlist, hlist->focus_row) < 0)
    gtk_hlist_moveto (hlist, hlist->focus_row, -1, 0, 0);

  update_extended_selection (hlist, hlist->focus_row);
}

static void
sync_selection (GtkHList *hlist,
		gint      row,
		gint      mode)
{
  GList *list;
  gint d;

  if (mode == SYNC_INSERT)
    d = 1;
  else
    d = -1;
      
  if (hlist->focus_row >= row)
    {
      if (d > 0 || hlist->focus_row > row)
	hlist->focus_row += d;
      if (hlist->focus_row == -1 && hlist->rows >= 1)
	hlist->focus_row = 0;
      else if (hlist->focus_row >= hlist->rows)
	hlist->focus_row = hlist->rows - 1;
    }

  GTK_HLIST_CLASS_FW (hlist)->resync_selection (hlist, NULL);

  g_list_free (hlist->undo_selection);
  g_list_free (hlist->undo_unselection);
  hlist->undo_selection = NULL;
  hlist->undo_unselection = NULL;

  hlist->anchor = -1;
  hlist->drag_pos = -1;
  hlist->undo_anchor = hlist->focus_row;

  list = hlist->selection;

  while (list)
    {
      if (GPOINTER_TO_INT (list->data) >= row)
	list->data = ((gchar*) list->data) + d;
      list = list->next;
    }
}

/* GTKOBJECT
 *   gtk_hlist_destroy
 *   gtk_hlist_finalize
 */
static void
gtk_hlist_destroy (GtkObject *object)
{
  gint i;
  GtkHList *hlist;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_HLIST (object));

  hlist = GTK_HLIST (object);

  /* freeze the list */
  hlist->freeze_count++;

  /* get rid of all the rows */
  gtk_hlist_clear (hlist);

  /* Since we don't have a _remove method, unparent the children
   * instead of destroying them so the focus will be unset properly.
   * (For other containers, the _remove method takes care of the
   * unparent) The destroy will happen when the refcount drops
   * to zero.
   */

  /* unref adjustments */
  if (hlist->hadjustment)
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (hlist->hadjustment), hlist);
      gtk_object_unref (GTK_OBJECT (hlist->hadjustment));
      hlist->hadjustment = NULL;
    }
  if (hlist->vadjustment)
    {
      gtk_signal_disconnect_by_data (GTK_OBJECT (hlist->vadjustment), hlist);
      gtk_object_unref (GTK_OBJECT (hlist->vadjustment));
      hlist->vadjustment = NULL;
    }

  remove_grab (hlist);

  /* destroy the column buttons */
  for (i = 0; i < hlist->columns; i++)
    if (hlist->column[i].button)
      {
	gtk_widget_unparent (hlist->column[i].button);
	hlist->column[i].button = NULL;
      }

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_hlist_finalize (GtkObject *object)
{
  GtkHList *hlist;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_HLIST (object));

  hlist = GTK_HLIST (object);

  columns_delete (hlist);

  g_mem_chunk_destroy (hlist->cell_mem_chunk);
  g_mem_chunk_destroy (hlist->row_mem_chunk);

  if (GTK_OBJECT_CLASS (parent_class)->finalize)
    (*GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

/* GTKWIDGET
 *   gtk_hlist_realize
 *   gtk_hlist_unrealize
 *   gtk_hlist_map
 *   gtk_hlist_unmap
 *   gtk_hlist_draw
 *   gtk_hlist_expose
 *   gtk_hlist_style_set
 *   gtk_hlist_key_press
 *   gtk_hlist_button_press
 *   gtk_hlist_button_release
 *   gtk_hlist_motion
 *   gtk_hlist_size_request
 *   gtk_hlist_size_allocate
 */

static GdkColor my_bg_color;
static GdkGC *my_bg_gc = 0;

static void
gtk_hlist_realize (GtkWidget *widget)
{
  GtkHList *hlist;
  GdkWindowAttr attributes;
  GdkGCValues values;
  GdkColormap *colormap;
  GdkColor color;
  GtkStyle *style;
  GtkHListRow *hlist_row;
  GList *list;
  gint attributes_mask;
  gint border_width;
  gint i;
  gint j;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HLIST (widget));

  hlist = GTK_HLIST (widget);

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

  border_width = GTK_CONTAINER (widget)->border_width;
  
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = widget->allocation.x + border_width;
  attributes.y = widget->allocation.y + border_width;
  attributes.width = widget->allocation.width - border_width * 2;
  attributes.height = widget->allocation.height - border_width * 2;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget);
  attributes.event_mask |= (GDK_EXPOSURE_MASK |
			    GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_KEY_PRESS_MASK |
			    GDK_KEY_RELEASE_MASK);
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  /* main window */
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget),
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, hlist);

  widget->style = gtk_style_attach (widget->style, widget->window);

  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);

  /* column-title window */

  attributes.x = hlist->column_title_area.x;
  attributes.y = hlist->column_title_area.y;
  attributes.width = hlist->column_title_area.width;
  attributes.height = hlist->column_title_area.height;
  
  hlist->title_window = gdk_window_new (widget->window, &attributes,
					attributes_mask);
  gdk_window_set_user_data (hlist->title_window, hlist);

  gtk_style_set_background (widget->style, hlist->title_window,
			    GTK_STATE_NORMAL);
  gdk_window_show (hlist->title_window);

  /* set things up so column buttons are drawn in title window */
  for (i = 0; i < hlist->columns; i++)
    if (hlist->column[i].button)
      gtk_widget_set_parent_window (hlist->column[i].button,
				    hlist->title_window);

  /* hlist-window */
  attributes.x = (hlist->internal_allocation.x +
		  widget->style->klass->xthickness);
  attributes.y = (hlist->internal_allocation.y +
		  widget->style->klass->ythickness +
		  hlist->column_title_area.height);
  attributes.width = hlist->hlist_window_width;
  attributes.height = hlist->hlist_window_height;
  
  hlist->hlist_window = gdk_window_new (widget->window, &attributes,
					attributes_mask);
  gdk_window_set_user_data (hlist->hlist_window, hlist);

  gdk_window_set_background (hlist->hlist_window,
			     &widget->style->base[GTK_STATE_NORMAL]);
  gdk_window_show (hlist->hlist_window);
  gdk_window_get_size (hlist->hlist_window, &hlist->hlist_window_width,
		       &hlist->hlist_window_height);

  /* create resize windows */
  attributes.wclass = GDK_INPUT_ONLY;
  attributes.event_mask = (GDK_BUTTON_PRESS_MASK |
			   GDK_BUTTON_RELEASE_MASK |
			   GDK_POINTER_MOTION_MASK |
			   GDK_POINTER_MOTION_HINT_MASK |
			   GDK_KEY_PRESS_MASK);
  attributes_mask = GDK_WA_CURSOR;
  attributes.cursor = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);
  hlist->cursor_drag = attributes.cursor;

  attributes.x =  LIST_WIDTH (hlist) + 1;
  attributes.y = 0;
  attributes.width = 0;
  attributes.height = 0;

  for (i = 0; i < hlist->columns; i++)
    {
      hlist->column[i].window = gdk_window_new (hlist->title_window,
						&attributes, attributes_mask);
      gdk_window_set_user_data (hlist->column[i].window, hlist);
    }

  /* This is slightly less efficient than creating them with the
   * right size to begin with, but easier
   */
  size_allocate_title_buttons (hlist);

  /* GCs */
  colormap = gtk_widget_get_colormap (widget);
  color.red = 0xd75c;
  color.green = 0xd75c;
  color.blue = 0xd75c;
  color.pixel = 0;
  if (!gdk_colormap_alloc_color(colormap, &color, 0, 1))
    g_warning("could not allocate color %x/%x/%x", color.red, color.green, color.blue);
  style = gtk_style_copy(gtk_widget_get_style(widget));
  my_bg_color = color;
  if (!my_bg_gc)
    my_bg_gc = gdk_gc_new(hlist->hlist_window);
  gdk_gc_set_foreground(my_bg_gc, &my_bg_color);
  gtk_widget_set_style(widget, style);
  gdk_window_set_background(hlist->hlist_window, &color);

  hlist->fg_gc = gdk_gc_new (widget->window);
  hlist->bg_gc = gdk_gc_new (widget->window);

  /* We'll use this gc to do scrolling as well */
  gdk_gc_set_exposures (hlist->fg_gc, TRUE);

  values.foreground = (widget->style->white.pixel==0 ?
		       widget->style->black:widget->style->white);
  values.function = GDK_XOR;
  values.subwindow_mode = GDK_INCLUDE_INFERIORS;
  hlist->xor_gc = gdk_gc_new_with_values (widget->window,
					  &values,
					  GDK_GC_FOREGROUND |
					  GDK_GC_FUNCTION |
					  GDK_GC_SUBWINDOW);

  /* attach optional row/cell styles, allocate foreground/background colors */
  list = hlist->row_list;
  for (i = 0; i < hlist->rows; i++)
    {
      hlist_row = list->data;
      list = list->next;

      if (hlist_row->style)
	hlist_row->style = gtk_style_attach (hlist_row->style,
					     hlist->hlist_window);

      if (hlist_row->fg_set || hlist_row->bg_set)
	{
	  colormap = gtk_widget_get_colormap (widget);
	  if (hlist_row->fg_set)
	    gdk_color_alloc (colormap, &hlist_row->foreground);
	  if (hlist_row->bg_set)
	    gdk_color_alloc (colormap, &hlist_row->background);
	}
      
      for (j = 0; j < hlist->columns; j++)
	if  (hlist_row->cell[j].style)
	  hlist_row->cell[j].style =
	    gtk_style_attach (hlist_row->cell[j].style, hlist->hlist_window);
    }
}

static void
gtk_hlist_unrealize (GtkWidget *widget)
{
  gint i;
  GtkHList *hlist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HLIST (widget));

  hlist = GTK_HLIST (widget);

  /* freeze the list */
  hlist->freeze_count++;

  if (GTK_WIDGET_MAPPED (widget))
    gtk_hlist_unmap (widget);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

  /* detach optional row/cell styles */
  if (GTK_WIDGET_REALIZED (widget))
    {
      GtkHListRow *hlist_row;
      GList *list;
      gint j;

      list = hlist->row_list;
      for (i = 0; i < hlist->rows; i++)
	{
	  hlist_row = list->data;
	  list = list->next;

	  if (hlist_row->style)
	    gtk_style_detach (hlist_row->style);
	  for (j = 0; j < hlist->columns; j++)
	    if  (hlist_row->cell[j].style)
	      gtk_style_detach (hlist_row->cell[j].style);
	}
    }

  gdk_cursor_destroy (hlist->cursor_drag);
  gdk_gc_destroy (hlist->xor_gc);
  gdk_gc_destroy (hlist->fg_gc);
  gdk_gc_destroy (hlist->bg_gc);

  for (i = 0; i < hlist->columns; i++)
    {
      if (hlist->column[i].button)
	gtk_widget_unrealize (hlist->column[i].button);
      if (hlist->column[i].window)
	{
	  gdk_window_set_user_data (hlist->column[i].window, NULL);
	  gdk_window_destroy (hlist->column[i].window);
	  hlist->column[i].window = NULL;
	}
    }

  gdk_window_set_user_data (hlist->hlist_window, NULL);
  gdk_window_destroy (hlist->hlist_window);
  hlist->hlist_window = NULL;

  gdk_window_set_user_data (hlist->title_window, NULL);
  gdk_window_destroy (hlist->title_window);
  hlist->title_window = NULL;

  hlist->cursor_drag = NULL;
  hlist->xor_gc = NULL;
  hlist->fg_gc = NULL;
  hlist->bg_gc = NULL;

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
gtk_hlist_map (GtkWidget *widget)
{
  gint i;
  GtkHList *hlist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HLIST (widget));

  hlist = GTK_HLIST (widget);

  if (!GTK_WIDGET_MAPPED (widget))
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);

      /* map column buttons */
      for (i = 0; i < hlist->columns; i++)
	{
	  if (hlist->column[i].button &&
	      GTK_WIDGET_VISIBLE (hlist->column[i].button) &&
	      !GTK_WIDGET_MAPPED (hlist->column[i].button))
	    gtk_widget_map (hlist->column[i].button);
	}
      
      for (i = 0; i < hlist->columns; i++)
	if (hlist->column[i].window && hlist->column[i].button)
	  {
	    gdk_window_raise (hlist->column[i].window);
	    gdk_window_show (hlist->column[i].window);
	  }

      gdk_window_show (hlist->title_window);
      gdk_window_show (hlist->hlist_window);
      gdk_window_show (widget->window);

      /* unfreeze the list */
      hlist->freeze_count = 0;
    }
}

static void
gtk_hlist_unmap (GtkWidget *widget)
{
  gint i;
  GtkHList *hlist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HLIST (widget));

  hlist = GTK_HLIST (widget);

  if (GTK_WIDGET_MAPPED (widget))
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

      if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (hlist))
	{
	  remove_grab (hlist);

	  GTK_HLIST_CLASS_FW (widget)->resync_selection (hlist, NULL);

	  hlist->click_cell.row = -1;
	  hlist->click_cell.column = -1;
	  hlist->drag_button = 0;

	  if (GTK_HLIST_IN_DRAG(hlist))
	    {
	      gpointer drag_data;

	      GTK_HLIST_UNSET_FLAG (hlist, HLIST_IN_DRAG);
	      drag_data = gtk_object_get_data (GTK_OBJECT (hlist),
					       "gtk-site-data");
	      if (drag_data)
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (hlist),
						    drag_data);
	    }
	}

      for (i = 0; i < hlist->columns; i++)
	if (hlist->column[i].window)
	  gdk_window_hide (hlist->column[i].window);

      gdk_window_hide (hlist->hlist_window);
      gdk_window_hide (hlist->title_window);
      gdk_window_hide (widget->window);

      /* unmap column buttons */
      for (i = 0; i < hlist->columns; i++)
	if (hlist->column[i].button &&
	    GTK_WIDGET_MAPPED (hlist->column[i].button))
	  gtk_widget_unmap (hlist->column[i].button);

      /* freeze the list */
      hlist->freeze_count++;
    }
}

static void
gtk_hlist_draw (GtkWidget    *widget,
		GdkRectangle *area)
{
  GtkHList *hlist;
  gint border_width;
  GdkRectangle child_area;
  int i;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HLIST (widget));
  g_return_if_fail (area != NULL);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      hlist = GTK_HLIST (widget);
      border_width = GTK_CONTAINER (widget)->border_width;

#if 0
      gdk_window_clear_area (widget->window,
			     area->x - border_width, 
			     area->y - border_width,
			     area->width, area->height);
#endif

      /* draw list shadow/border */
      gtk_draw_shadow (widget->style, widget->window,
		       GTK_STATE_NORMAL, hlist->shadow_type,
		       0, 0, 
		       hlist->hlist_window_width +
		       (2 * widget->style->klass->xthickness),
		       hlist->hlist_window_height +
		       (2 * widget->style->klass->ythickness) +
		       hlist->column_title_area.height);

#if 0
      gdk_window_clear_area (hlist->hlist_window, 0, 0, 0, 0);
#endif
      draw_rows (hlist, area);

      for (i = 0; i < hlist->columns; i++)
	{
	  if (!hlist->column[i].visible)
	    continue;
	  if (hlist->column[i].button &&
	      gtk_widget_intersect(hlist->column[i].button, area, &child_area))
	    gtk_widget_draw (hlist->column[i].button, &child_area);
	}
    }
}

static gint
gtk_hlist_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  GtkHList *hlist;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      hlist = GTK_HLIST (widget);

      /* draw border */
      if (event->window == widget->window)
	gtk_draw_shadow (widget->style, widget->window,
			 GTK_STATE_NORMAL, hlist->shadow_type,
			 0, 0,
			 hlist->hlist_window_width +
			 (2 * widget->style->klass->xthickness),
			 hlist->hlist_window_height +
			 (2 * widget->style->klass->ythickness) +
			 hlist->column_title_area.height);

      /* exposure events on the list */
      if (event->window == hlist->hlist_window)
	draw_rows (hlist, &event->area);
    }

  return FALSE;
}

static void
gtk_hlist_style_set (GtkWidget *widget,
		     GtkStyle  *previous_style)
{
  GtkHList *hlist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HLIST (widget));

  if (GTK_WIDGET_CLASS (parent_class)->style_set)
    (*GTK_WIDGET_CLASS (parent_class)->style_set) (widget, previous_style);

  hlist = GTK_HLIST (widget);

  if (GTK_WIDGET_REALIZED (widget))
    {
      gtk_style_set_background (widget->style, widget->window, widget->state);
      gtk_style_set_background (widget->style, hlist->title_window, GTK_STATE_SELECTED);
      gdk_window_set_background (hlist->hlist_window, &widget->style->base[GTK_STATE_NORMAL]);
    }

  /* Fill in data after widget has correct style */

  /* text properties */
  if (!GTK_HLIST_ROW_HEIGHT_SET(hlist))
    {
      hlist->row_height[0] = (widget->style->font->ascent +
			   widget->style->font->descent + 1);
      hlist->row_center_offset = widget->style->font->ascent + 1.5;
    }
  else
    hlist->row_center_offset = 1.5 + (hlist->row_height[0] +
				      widget->style->font->ascent -
				      widget->style->font->descent - 1) / 2;

  /* Column widths */
  if (!GTK_HLIST_AUTO_RESIZE_BLOCKED(hlist))
    {
      gint width;
      gint i;

      for (i = 0; i < hlist->columns; i++)
	if (hlist->column[i].auto_resize)
	  {
	    width = gtk_hlist_optimal_column_width (hlist, i);
	    if (width != hlist->column[i].width)
	      gtk_hlist_set_column_width (hlist, i, width);
	  }
    }
}

static gint
gtk_hlist_key_press (GtkWidget   *widget,
		     GdkEventKey *event)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_CLASS (parent_class)->key_press_event &&
      GTK_WIDGET_CLASS (parent_class)->key_press_event (widget, event))
    return TRUE;

  switch (event->keyval)
    {
    case GDK_Tab:
    case GDK_ISO_Left_Tab:
      if (event->state & GDK_SHIFT_MASK)
	return gtk_container_focus (GTK_CONTAINER (widget),
				    GTK_DIR_TAB_BACKWARD);
      else
	return gtk_container_focus (GTK_CONTAINER (widget),
				    GTK_DIR_TAB_FORWARD);
    default:
      break;
    }
  return FALSE;
}

static gint
gtk_hlist_button_press (GtkWidget      *widget,
			GdkEventButton *event)
{
  gint i;
  GtkHList *hlist;
  gint x;
  gint y;
  gint row;
  gint column;
  gint button_actions;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  hlist = GTK_HLIST (widget);

  button_actions = hlist->button_actions[event->button - 1];

  if (button_actions == GTK_BUTTON_IGNORED)
    return FALSE;

  /* selections on the list */
  if (event->window == hlist->hlist_window)
    {
      x = event->x;
      y = event->y;

      if (get_selection_info (hlist, x, y, &row, &column))
	{
	  gint old_row = hlist->focus_row;

	  if (hlist->focus_row == -1)
	    old_row = row;

	  if (event->type == GDK_BUTTON_PRESS)
	    {
	      GdkEventMask mask = ((1 << (4 + event->button)) |
				   GDK_POINTER_MOTION_HINT_MASK |
				   GDK_BUTTON_RELEASE_MASK);

	      if (gdk_pointer_grab (hlist->hlist_window, FALSE, mask,
				    NULL, NULL, event->time))
		return FALSE;
	      gtk_grab_add (widget);

	      hlist->click_cell.row = row;
	      hlist->click_cell.column = column;
	      hlist->drag_button = event->button;
	    }
	  else
	    {
	      hlist->click_cell.row = -1;
	      hlist->click_cell.column = -1;

	      hlist->drag_button = 0;
	      remove_grab (hlist);
	    }

	  if (button_actions & GTK_BUTTON_SELECTS)
	    {
	      if (GTK_HLIST_ADD_MODE(hlist))
		{
		  GTK_HLIST_UNSET_FLAG (hlist, HLIST_ADD_MODE);
		  if (GTK_WIDGET_HAS_FOCUS(widget))
		    {
		      gtk_hlist_draw_focus (widget);
		      gdk_gc_set_line_attributes (hlist->xor_gc, 1,
						  GDK_LINE_SOLID, 0, 0);
		      hlist->focus_row = row;
		      gtk_hlist_draw_focus (widget);
		    }
		  else
		    {
		      gdk_gc_set_line_attributes (hlist->xor_gc, 1,
						  GDK_LINE_SOLID, 0, 0);
		      hlist->focus_row = row;
		    }
		}
	      else if (row != hlist->focus_row)
		{
		  if (GTK_WIDGET_HAS_FOCUS(widget))
		    {
		      gtk_hlist_draw_focus (widget);
		      hlist->focus_row = row;
		      gtk_hlist_draw_focus (widget);
		    }
		  else
		    hlist->focus_row = row;
		}
	    }

	  if (!GTK_WIDGET_HAS_FOCUS(widget))
	    gtk_widget_grab_focus (widget);

	  if (button_actions & GTK_BUTTON_SELECTS)
	    {
	      switch (hlist->selection_mode)
		{
		case GTK_SELECTION_SINGLE:
		case GTK_SELECTION_MULTIPLE:
		  if (event->type != GDK_BUTTON_PRESS)
		    {
		      gtk_signal_emit (GTK_OBJECT (hlist),
				       hlist_signals[SELECT_ROW],
				       row, column, event);
		      hlist->anchor = -1;
		    }
		  else
		    hlist->anchor = row;
		  break;
		case GTK_SELECTION_BROWSE:
		  gtk_signal_emit (GTK_OBJECT (hlist),
				   hlist_signals[SELECT_ROW],
				   row, column, event);
		  break;
		case GTK_SELECTION_EXTENDED:
		  if (event->type != GDK_BUTTON_PRESS)
		    {
		      if (hlist->anchor != -1)
			{
			  update_extended_selection (hlist, hlist->focus_row);
			  GTK_HLIST_CLASS_FW (hlist)->resync_selection
			    (hlist, (GdkEvent *) event);
			}
		      gtk_signal_emit (GTK_OBJECT (hlist),
				       hlist_signals[SELECT_ROW],
				       row, column, event);
		      break;
		    }
	      
		  if (event->state & GDK_CONTROL_MASK)
		    {
		      if (event->state & GDK_SHIFT_MASK)
			{
			  if (hlist->anchor < 0)
			    {
			      g_list_free (hlist->undo_selection);
			      g_list_free (hlist->undo_unselection);
			      hlist->undo_selection = NULL;
			      hlist->undo_unselection = NULL;
			      hlist->anchor = old_row;
			      hlist->drag_pos = old_row;
			      hlist->undo_anchor = old_row;
			    }
			  update_extended_selection (hlist, hlist->focus_row);
			}
		      else
			{
			  if (hlist->anchor == -1)
			    set_anchor (hlist, TRUE, row, old_row);
			  else
			    update_extended_selection (hlist,
						       hlist->focus_row);
			}
		      break;
		    }

		  if (event->state & GDK_SHIFT_MASK)
		    {
		      set_anchor (hlist, FALSE, old_row, old_row);
		      update_extended_selection (hlist, hlist->focus_row);
		      break;
		    }

		  if (hlist->anchor == -1)
		    set_anchor (hlist, FALSE, row, old_row);
		  else
		    update_extended_selection (hlist, hlist->focus_row);
		  break;
		default:
		  break;
		}
	    }
	}
      return FALSE;
    }

  /* press on resize windows */
  for (i = 0; i < hlist->columns; i++)
    if (hlist->column[i].resizeable && hlist->column[i].window &&
	event->window == hlist->column[i].window)
      {
	gpointer drag_data;

	if (gdk_pointer_grab (hlist->column[i].window, FALSE,
			      GDK_POINTER_MOTION_HINT_MASK |
			      GDK_BUTTON1_MOTION_MASK |
			      GDK_BUTTON_RELEASE_MASK,
			      NULL, NULL, event->time))
	  return FALSE;

	gtk_grab_add (widget);
	GTK_HLIST_SET_FLAG (hlist, HLIST_IN_DRAG);

	/* block attached dnd signal handler */
	drag_data = gtk_object_get_data (GTK_OBJECT (hlist), "gtk-site-data");
	if (drag_data)
	  gtk_signal_handler_block_by_data (GTK_OBJECT (hlist), drag_data);

	if (!GTK_WIDGET_HAS_FOCUS(widget))
	  gtk_widget_grab_focus (widget);

	hlist->drag_pos = i;
	hlist->x_drag = (COLUMN_LEFT_XPIXEL(hlist, i) + COLUMN_INSET +
			 hlist->column[i].area.width + HELL_SPACING);

	if (GTK_HLIST_ADD_MODE(hlist))
	  gdk_gc_set_line_attributes (hlist->xor_gc, 1, GDK_LINE_SOLID, 0, 0);
	draw_xor_line (hlist);
      }
  return FALSE;
}

static gint
gtk_hlist_button_release (GtkWidget      *widget,
			  GdkEventButton *event)
{
  GtkHList *hlist;
  gint button_actions;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  hlist = GTK_HLIST (widget);

  button_actions = hlist->button_actions[event->button - 1];
  if (button_actions == GTK_BUTTON_IGNORED)
    return FALSE;

  /* release on resize windows */
  if (GTK_HLIST_IN_DRAG(hlist))
    {
      gpointer drag_data;
      gint width;
      gint x;
      gint i;

      i = hlist->drag_pos;
      hlist->drag_pos = -1;

      /* unblock attached dnd signal handler */
      drag_data = gtk_object_get_data (GTK_OBJECT (hlist), "gtk-site-data");
      if (drag_data)
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (hlist), drag_data);

      GTK_HLIST_UNSET_FLAG (hlist, HLIST_IN_DRAG);
      gtk_widget_get_pointer (widget, &x, NULL);
      gtk_grab_remove (widget);
      gdk_pointer_ungrab (event->time);

      if (hlist->x_drag >= 0)
	draw_xor_line (hlist);

      if (GTK_HLIST_ADD_MODE(hlist))
	{
	  gdk_gc_set_line_attributes (hlist->xor_gc, 1,
				      GDK_LINE_ON_OFF_DASH, 0, 0);
	  gdk_gc_set_dashes (hlist->xor_gc, 0, "\4\4", 2);
	}

      width = new_column_width (hlist, i, &x);
      gtk_hlist_set_column_width (hlist, i, width);
      return FALSE;
    }

  if (hlist->drag_button == event->button)
    {
      gint row;
      gint column;

      hlist->drag_button = 0;
      hlist->click_cell.row = -1;
      hlist->click_cell.column = -1;

      remove_grab (hlist);

      if (button_actions & GTK_BUTTON_SELECTS)
	{
	  switch (hlist->selection_mode)
	    {
	    case GTK_SELECTION_EXTENDED:
	      if (!(event->state & GDK_SHIFT_MASK) ||
		  !GTK_WIDGET_CAN_FOCUS (widget) ||
		  event->x < 0 || event->x >= hlist->hlist_window_width ||
		  event->y < 0 || event->y >= hlist->hlist_window_height)
		GTK_HLIST_CLASS_FW (hlist)->resync_selection
		  (hlist, (GdkEvent *) event);
	      break;
	    case GTK_SELECTION_SINGLE:
	    case GTK_SELECTION_MULTIPLE:
	      if (get_selection_info (hlist, event->x, event->y,
				      &row, &column))
		{
		  if (row >= 0 && row < hlist->rows && hlist->anchor == row)
		    toggle_row (hlist, row, column, (GdkEvent *) event);
		}
	      hlist->anchor = -1;
	      break;
	    default:
	      break;
	    }
	}
    }
  return FALSE;
}

static gint
gtk_hlist_motion (GtkWidget      *widget,
		  GdkEventMotion *event)
{
  GtkHList *hlist;
  gint x;
  gint y;
  gint row;
  gint new_width;
  gint button_actions = 0;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HLIST (widget), FALSE);

  hlist = GTK_HLIST (widget);
  if (!(gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (hlist)))
    return FALSE;

  if (hlist->drag_button > 0)
    button_actions = hlist->button_actions[hlist->drag_button - 1];

  if (GTK_HLIST_IN_DRAG(hlist))
    {
      if (event->is_hint || event->window != widget->window)
	gtk_widget_get_pointer (widget, &x, NULL);
      else
	x = event->x;
      
      new_width = new_column_width (hlist, hlist->drag_pos, &x);
      if (x != hlist->x_drag)
	{
	  /* x_drag < 0 indicates that the xor line is already invisible */
	  if (hlist->x_drag >= 0)
	    draw_xor_line (hlist);

	  hlist->x_drag = x;

	  if (hlist->x_drag >= 0)
	    draw_xor_line (hlist);
	}

      if (new_width <= MAX (COLUMN_MIN_WIDTH + 1,
			    hlist->column[hlist->drag_pos].min_width + 1))
	{
	  if (COLUMN_LEFT_XPIXEL (hlist, hlist->drag_pos) < 0 && x < 0)
	    gtk_hlist_moveto (hlist, -1, hlist->drag_pos, 0, 0);
	  return FALSE;
	}
      if (hlist->column[hlist->drag_pos].max_width >= COLUMN_MIN_WIDTH &&
	  new_width >= hlist->column[hlist->drag_pos].max_width)
	{
	  if (COLUMN_LEFT_XPIXEL (hlist, hlist->drag_pos) + new_width >
	      hlist->hlist_window_width && x < 0)
	    move_horizontal (hlist,
			     COLUMN_LEFT_XPIXEL (hlist, hlist->drag_pos) +
			     new_width - hlist->hlist_window_width +
			     COLUMN_INSET + HELL_SPACING);
	  return FALSE;
	}
    }

  if (event->is_hint || event->window != hlist->hlist_window)
    gdk_window_get_pointer (hlist->hlist_window, &x, &y, NULL);

  if (GTK_HLIST_REORDERABLE(hlist) && button_actions & GTK_BUTTON_DRAGS)
    {
      /* delayed drag start */
      if (event->window == hlist->hlist_window &&
	  hlist->click_cell.row >= 0 && hlist->click_cell.column >= 0 &&
	  (y < 0 || y >= hlist->hlist_window_height ||
	   x < 0 || x >= hlist->hlist_window_width  ||
	   y < ROW_TOP_YPIXEL (hlist, hlist->click_cell.row) ||
	   y >= (ROW_TOP_YPIXEL (hlist, hlist->click_cell.row) +
		 hlist->row_height[hlist->click_cell.row]) ||
	   x < COLUMN_LEFT_XPIXEL (hlist, hlist->click_cell.column) ||
	   x >= (COLUMN_LEFT_XPIXEL(hlist, hlist->click_cell.column) + 
		 hlist->column[hlist->click_cell.column].area.width)))
	{
	  GtkTargetList  *target_list;

	  target_list = gtk_target_list_new (&hlist_target_table, 1);
	  gtk_drag_begin (widget, target_list, GDK_ACTION_MOVE,
			  hlist->drag_button, (GdkEvent *)event);

	}
      return TRUE;
    }

  /* horizontal autoscrolling */
  if (hlist->hadjustment && LIST_WIDTH (hlist) > hlist->hlist_window_width &&
      (x < 0 || x >= hlist->hlist_window_width))
    {
      if (hlist->htimer)
	return FALSE;

      hlist->htimer = gtk_timeout_add
	(SCROLL_TIME, (GtkFunction) horizontal_timeout, hlist);

      if (!((x < 0 && hlist->hadjustment->value == 0) ||
	    (x >= hlist->hlist_window_width &&
	     hlist->hadjustment->value ==
	     LIST_WIDTH (hlist) - hlist->hlist_window_width)))
	{
	  if (x < 0)
	    move_horizontal (hlist, -1 + (x/2));
	  else
	    move_horizontal (hlist, 1 + (x - hlist->hlist_window_width) / 2);
	}
    }

  if (GTK_HLIST_IN_DRAG(hlist))
    return FALSE;

  /* vertical autoscrolling */
  row = ROW_FROM_YPIXEL (hlist, y);

  /* don't scroll on last pixel row if it's a cell spacing */
  if (y == hlist->hlist_window_height - 1 &&
      y == ROW_TOP_YPIXEL (hlist, row-1) + hlist->row_height[row])
    return FALSE;

  if (LIST_HEIGHT (hlist) > hlist->hlist_window_height &&
      (y < 0 || y >= hlist->hlist_window_height))
    {
      if (hlist->vtimer)
	return FALSE;

      hlist->vtimer = gtk_timeout_add (SCROLL_TIME,
				       (GtkFunction) vertical_timeout, hlist);

      if (hlist->drag_button &&
	  ((y < 0 && hlist->focus_row == 0) ||
	   (y >= hlist->hlist_window_height &&
	    hlist->focus_row == hlist->rows - 1)))
	return FALSE;
    }

  row = CLAMP (row, 0, hlist->rows - 1);

  if (button_actions & GTK_BUTTON_SELECTS &
      !gtk_object_get_data (GTK_OBJECT (widget), "gtk-site-data"))
    {
      if (row == hlist->focus_row)
	return FALSE;

      gtk_hlist_draw_focus (widget);
      hlist->focus_row = row;
      gtk_hlist_draw_focus (widget);

      switch (hlist->selection_mode)
	{
	case GTK_SELECTION_BROWSE:
	  gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[SELECT_ROW],
			   hlist->focus_row, -1, event);
	  break;
	case GTK_SELECTION_EXTENDED:
	  update_extended_selection (hlist, hlist->focus_row);
	  break;
	default:
	  break;
	}
    }
  
  if (ROW_TOP_YPIXEL(hlist, row) < 0)
    move_vertical (hlist, row, 0);
  else if (ROW_TOP_YPIXEL(hlist, row) + hlist->row_height[row] >
	   hlist->hlist_window_height)
    move_vertical (hlist, row, 1);

  return FALSE;
}

static void
gtk_hlist_size_request (GtkWidget      *widget,
			GtkRequisition *requisition)
{
  GtkHList *hlist;
  gint i;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HLIST (widget));
  g_return_if_fail (requisition != NULL);

  hlist = GTK_HLIST (widget);

  requisition->width = 0;
  requisition->height = 0;

  /* compute the size of the column title (title) area */
  hlist->column_title_area.height = 0;
  if (GTK_HLIST_SHOW_TITLES(hlist))
    for (i = 0; i < hlist->columns; i++)
      if (hlist->column[i].button)
	{
	  GtkRequisition child_requisition;
	  
	  gtk_widget_size_request (hlist->column[i].button,
				   &child_requisition);
	  hlist->column_title_area.height =
	    MAX (hlist->column_title_area.height,
		 child_requisition.height);
	}

  requisition->width += (widget->style->klass->xthickness +
			 GTK_CONTAINER (widget)->border_width) * 2;
  requisition->height += (hlist->column_title_area.height +
			  (widget->style->klass->ythickness +
			   GTK_CONTAINER (widget)->border_width) * 2);

  /* if (!hlist->hadjustment) */
  requisition->width += list_requisition_width (hlist);
  /* if (!hlist->vadjustment) */
  requisition->height += LIST_HEIGHT (hlist);
}

static void
gtk_hlist_size_allocate (GtkWidget     *widget,
			 GtkAllocation *allocation)
{
  GtkHList *hlist;
  GtkAllocation hlist_allocation;
  gint border_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HLIST (widget));
  g_return_if_fail (allocation != NULL);

  hlist = GTK_HLIST (widget);
  widget->allocation = *allocation;
  border_width = GTK_CONTAINER (widget)->border_width;

  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (widget->window,
			      allocation->x + border_width,
			      allocation->y + border_width,
			      allocation->width - border_width * 2,
			      allocation->height - border_width * 2);
    }

  /* use internal allocation structure for all the math
   * because it's easier than always subtracting the container
   * border width */
  hlist->internal_allocation.x = 0;
  hlist->internal_allocation.y = 0;
  hlist->internal_allocation.width = MAX (1, (gint)allocation->width -
					  border_width * 2);
  hlist->internal_allocation.height = MAX (1, (gint)allocation->height -
					   border_width * 2);
	
  /* allocate hlist window assuming no scrollbars */
  hlist_allocation.x = (hlist->internal_allocation.x +
			widget->style->klass->xthickness);
  hlist_allocation.y = (hlist->internal_allocation.y +
			widget->style->klass->ythickness +
			hlist->column_title_area.height);
  hlist_allocation.width = MAX (1, (gint)hlist->internal_allocation.width - 
				(2 * (gint)widget->style->klass->xthickness));
  hlist_allocation.height = MAX (1, (gint)hlist->internal_allocation.height -
				 (2 * (gint)widget->style->klass->ythickness) -
				 (gint)hlist->column_title_area.height);
  
  hlist->hlist_window_width = hlist_allocation.width;
  hlist->hlist_window_height = hlist_allocation.height;
  
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (hlist->hlist_window,
			      hlist_allocation.x,
			      hlist_allocation.y,
			      hlist_allocation.width,
			      hlist_allocation.height);
    }
  
  /* position the window which holds the column title buttons */
  hlist->column_title_area.x = widget->style->klass->xthickness;
  hlist->column_title_area.y = widget->style->klass->ythickness;
  hlist->column_title_area.width = hlist_allocation.width;
  
  if (GTK_WIDGET_REALIZED (widget))
    {
      gdk_window_move_resize (hlist->title_window,
			      hlist->column_title_area.x,
			      hlist->column_title_area.y,
			      hlist->column_title_area.width,
			      hlist->column_title_area.height);
    }
  
  /* column button allocation */
  size_allocate_columns (hlist, FALSE);
  size_allocate_title_buttons (hlist);

  adjust_adjustments (hlist, TRUE);
}

/* GTKCONTAINER
 *   gtk_hlist_forall
 */
static void
gtk_hlist_forall (GtkContainer *container,
		  gboolean      include_internals,
		  GtkCallback   callback,
		  gpointer      callback_data)
{
  GtkHList *hlist;
  gint i;

  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_HLIST (container));
  g_return_if_fail (callback != NULL);

  if (!include_internals)
    return;

  hlist = GTK_HLIST (container);
      
  /* callback for the column buttons */
  for (i = 0; i < hlist->columns; i++)
    if (hlist->column[i].button)
      (*callback) (hlist->column[i].button, callback_data);
}

/* PRIVATE DRAWING FUNCTIONS
 *   get_cell_style
 *   draw_cell_pixmap
 *   draw_row
 *   draw_rows
 *   draw_xor_line
 *   hlist_refresh
 */
static void
get_cell_style (GtkHList     *hlist,
		GtkHListRow  *hlist_row,
		gint          state,
		gint          column,
		GtkStyle    **style,
		GdkGC       **fg_gc,
		GdkGC       **bg_gc)
{
  gint fg_state;

  if ((state == GTK_STATE_NORMAL) &&
      (GTK_WIDGET (hlist)->state == GTK_STATE_INSENSITIVE))
    fg_state = GTK_STATE_INSENSITIVE;
  else
    fg_state = state;

  if (hlist_row->cell[column].style)
    {
      if (style)
	*style = hlist_row->cell[column].style;
      if (fg_gc)
	*fg_gc = hlist_row->cell[column].style->fg_gc[fg_state];
      if (bg_gc) {
	if (state == GTK_STATE_SELECTED)
	  *bg_gc = hlist_row->cell[column].style->bg_gc[state];
	else
	  *bg_gc = hlist_row->cell[column].style->base_gc[state];
      }
    }
  else if (hlist_row->style)
    {
      if (style)
	*style = hlist_row->style;
      if (fg_gc)
	*fg_gc = hlist_row->style->fg_gc[fg_state];
      if (bg_gc) {
	if (state == GTK_STATE_SELECTED)
	  *bg_gc = hlist_row->style->bg_gc[state];
	else
	  *bg_gc = hlist_row->style->base_gc[state];
      }
    }
  else
    {
      if (style)
	*style = GTK_WIDGET (hlist)->style;
      if (fg_gc)
	*fg_gc = GTK_WIDGET (hlist)->style->fg_gc[fg_state];
      if (bg_gc) {
	if (state == GTK_STATE_SELECTED)
	  *bg_gc = GTK_WIDGET (hlist)->style->bg_gc[state];
	else
	  *bg_gc = my_bg_gc;
      }

      if (fg_gc && hlist_row->fg_set)
	*fg_gc = hlist->fg_gc;
      if (state != GTK_STATE_SELECTED) {
	if (bg_gc && hlist_row->bg_set)
	  *bg_gc = hlist->bg_gc;
      }
    }
}

static gint
draw_cell_pixmap (GdkWindow    *window,
		  GdkRectangle *clip_rectangle,
		  GdkGC        *fg_gc,
		  GdkGC        *fill_gc,
		  GdkPixmap    *pixmap,
		  GdkBitmap    *mask,
		  gint          x,
		  gint          y,
		  gint          width,
		  gint          height)
{
  gint xsrc = 0;
  gint ysrc = 0;

  if (mask)
    {
      gdk_gc_set_clip_mask (fg_gc, mask);
      gdk_gc_set_clip_origin (fg_gc, x, y);
      if (fill_gc) {
	gdk_gc_set_clip_mask (fill_gc, mask);
	gdk_gc_set_clip_origin (fill_gc, x, y);
      }
    }

  if (x < clip_rectangle->x)
    {
      xsrc = clip_rectangle->x - x;
      width -= xsrc;
      x = clip_rectangle->x;
    }
  if (x + width > clip_rectangle->x + clip_rectangle->width)
    width = clip_rectangle->x + clip_rectangle->width - x;

  if (y < clip_rectangle->y)
    {
      ysrc = clip_rectangle->y - y;
      height -= ysrc;
      y = clip_rectangle->y;
    }
  if (y + height > clip_rectangle->y + clip_rectangle->height)
    height = clip_rectangle->y + clip_rectangle->height - y;
  gdk_draw_pixmap (window, fg_gc, pixmap, xsrc, ysrc, x, y, width, height);
  if (fill_gc)
    gdk_draw_rectangle (window, fill_gc, TRUE, x, y, width, height);
  gdk_gc_set_clip_origin (fg_gc, 0, 0);
  if (mask)
    gdk_gc_set_clip_mask (fg_gc, NULL);
  if (fill_gc) {
    gdk_gc_set_clip_origin (fill_gc, 0, 0);
    if (mask)
      gdk_gc_set_clip_mask (fill_gc, NULL);
  }

  return x + MAX (width, 0);
}

void
gtk_hlist_set_stipple (GtkHList *hlist)
{
	GtkWidget *widget;
	GdkBitmap *stipple;
	GdkVisual *visual;
	GdkImage *bimage;
	GdkGC *fill_gc;
	char data[16*16>>3];
	char *p;

	widget = GTK_WIDGET(hlist);
	for (p = data; p < data+32; ) {
		*p++ = 170; *p++ = 170;
		*p++ = 85; *p++ = 85;
	}
	visual = gtk_widget_get_visual(widget);
	bimage = gdk_image_new_bitmap(visual, data, 16, 16);
	stipple = gdk_pixmap_new(hlist->hlist_window, 16, 16, 1);
	fill_gc = gdk_gc_new(stipple);
	gdk_draw_image(stipple, fill_gc, bimage, 0, 0, 0, 0, 16, 16);
	gdk_gc_unref(fill_gc);
	gdk_bitmap_ref(stipple);
	fill_gc = gdk_gc_new(hlist->hlist_window);
	gdk_gc_copy(fill_gc, widget->style->bg_gc[GTK_STATE_SELECTED]);
	if (stipple) {
		gdk_gc_set_stipple(fill_gc, stipple);
		gdk_gc_set_fill(fill_gc, GDK_STIPPLED);
		gdk_gc_set_ts_origin(fill_gc, 0, 0);
	}
	hlist->stipple_bitmap = stipple;
	hlist->stipple_gc = fill_gc;
}

static void
draw_row (GtkHList     *hlist,
	  GdkRectangle *area,
	  gint          row,
	  GtkHListRow  *hlist_row)
{
  GtkWidget *widget;
  GdkRectangle *rect;
  GdkRectangle row_rectangle;
  GdkRectangle cell_rectangle;
  GdkRectangle clip_rectangle;
  GdkRectangle intersect_rectangle;
  gint last_column;
  gint state;
  gint i;
  GdkGC *fill_gc;

  g_return_if_fail (hlist != NULL);

  /* bail now if we arn't drawable yet */
  if (!GTK_WIDGET_DRAWABLE (hlist) || row < 0 || row >= hlist->rows)
    return;

  widget = GTK_WIDGET (hlist);

  if (!hlist->stipple_gc && hlist->want_stipple)
    gtk_hlist_set_stipple(hlist);

  /* if the function is passed the pointer to the row instead of null,
   * it avoids this expensive lookup */
  if (!hlist_row)
    hlist_row = (g_list_nth (hlist->row_list, row))->data;

  /* rectangle of the entire row */
  row_rectangle.x = 0;
  row_rectangle.y = ROW_TOP_YPIXEL (hlist, row);
  row_rectangle.width = hlist->hlist_window_width;
  row_rectangle.height = hlist->row_height[row];

  /* rectangle of the cell spacing above the row */
  cell_rectangle.x = 0;
  cell_rectangle.y = row_rectangle.y - HELL_SPACING;
  cell_rectangle.width = row_rectangle.width;
  cell_rectangle.height = HELL_SPACING;

  /* rectangle used to clip drawing operations, it's y and height
   * positions only need to be set once, so we set them once here. 
   * the x and width are set withing the drawing loop below once per
   * column */
  clip_rectangle.y = row_rectangle.y;
  clip_rectangle.height = row_rectangle.height;

  if (hlist_row->fg_set)
    gdk_gc_set_foreground (hlist->fg_gc, &hlist_row->foreground);
  if (hlist_row->state == GTK_STATE_NORMAL)
    {
      if (hlist_row->bg_set)
	gdk_gc_set_foreground (hlist->bg_gc, &hlist_row->background);
    }

  state = hlist_row->state;

  /* draw the cell borders and background */
  if (area)
    {
      rect = &intersect_rectangle;
      if (gdk_rectangle_intersect (area, &cell_rectangle,
				   &intersect_rectangle))
	gdk_draw_rectangle (hlist->hlist_window,
			    widget->style->base_gc[GTK_STATE_ACTIVE],
			    TRUE,
			    intersect_rectangle.x,
			    intersect_rectangle.y,
			    intersect_rectangle.width,
			    intersect_rectangle.height);

      /* the last row has to clear it's bottom cell spacing too */
      if (hlist_row == hlist->row_list_end->data)
	{
	  cell_rectangle.y += hlist->row_height[row] + HELL_SPACING;

	  if (gdk_rectangle_intersect (area, &cell_rectangle,
				       &intersect_rectangle))
	    gdk_draw_rectangle (hlist->hlist_window,
				widget->style->base_gc[GTK_STATE_ACTIVE],
				TRUE,
				intersect_rectangle.x,
				intersect_rectangle.y,
				intersect_rectangle.width,
				intersect_rectangle.height);
	}

      if (!gdk_rectangle_intersect (area, &row_rectangle,&intersect_rectangle))
	return;

    }
  else
    {
      rect = &clip_rectangle;
      gdk_draw_rectangle (hlist->hlist_window,
			  widget->style->base_gc[GTK_STATE_ACTIVE],
			  TRUE,
			  cell_rectangle.x,
			  cell_rectangle.y,
			  cell_rectangle.width,
			  cell_rectangle.height);

      /* the last row has to clear it's bottom cell spacing too */
      if (hlist_row == hlist->row_list_end->data)
	{
	  cell_rectangle.y += hlist->row_height[row] + HELL_SPACING;

	  gdk_draw_rectangle (hlist->hlist_window,
			      widget->style->base_gc[GTK_STATE_ACTIVE],
			      TRUE,
			      cell_rectangle.x,
			      cell_rectangle.y,
			      cell_rectangle.width,
			      cell_rectangle.height);     
	}	  
    }
  
  for (last_column = hlist->columns - 1;
       last_column >= 0 && !hlist->column[last_column].visible; last_column--)
    ;

  /* iterate and draw all the columns (row cells) and draw their contents */
  for (i = 0; i < hlist->columns; i++)
    {
      GtkStyle *style;
      GdkGC *fg_gc;
      GdkGC *bg_gc;

      gint width;
      gint height;
      gint pixmap_width;
      gint offset = 0;
      gint row_center_offset;

      if (!hlist->column[i].visible)
	continue;

      get_cell_style (hlist, hlist_row, state, i, &style, &fg_gc, &bg_gc);

      clip_rectangle.x = hlist->column[i].area.x + hlist->hoffset;
      clip_rectangle.width = hlist->column[i].area.width;

      /* calculate clipping region clipping region */
      clip_rectangle.x -= COLUMN_INSET + HELL_SPACING;
      clip_rectangle.width += (2 * COLUMN_INSET + HELL_SPACING +
			       (i == last_column) * HELL_SPACING);
      
      if (area && !gdk_rectangle_intersect (area, &clip_rectangle,
					    &intersect_rectangle))
	continue;

      gdk_window_clear_area (hlist->hlist_window, rect->x, rect->y, rect->width, rect->height);

      if (hlist->stipple_gc && bg_gc == widget->style->bg_gc[GTK_STATE_SELECTED])
	gdk_draw_rectangle (hlist->hlist_window, hlist->stipple_gc, TRUE,
			    rect->x, rect->y, rect->width, rect->height);
      else
	gdk_draw_rectangle (hlist->hlist_window, bg_gc, TRUE,
			    rect->x, rect->y, rect->width, rect->height);

      clip_rectangle.x += COLUMN_INSET + HELL_SPACING;
      clip_rectangle.width -= (2 * COLUMN_INSET + HELL_SPACING +
			       (i == last_column) * HELL_SPACING);

      /* calculate real width for column justification */
      pixmap_width = 0;
      offset = 0;
      switch (hlist_row->cell[i].type)
	{
	case GTK_HELL_TEXT:
	  width = gdk_string_width (style->font,
				    GTK_HELL_TEXT (hlist_row->cell[i])->text);
	  break;
	case GTK_HELL_PIXMAP:
	  gdk_window_get_size (GTK_HELL_PIXMAP (hlist_row->cell[i])->pixmap,
			       &pixmap_width, &height);
	  width = pixmap_width;
	  break;
	case GTK_HELL_PIXTEXT:
	  gdk_window_get_size (GTK_HELL_PIXTEXT (hlist_row->cell[i])->pixmap,
			       &pixmap_width, &height);
	  width = (pixmap_width +
		   GTK_HELL_PIXTEXT (hlist_row->cell[i])->spacing +
		   gdk_string_width (style->font,
				     GTK_HELL_PIXTEXT
				     (hlist_row->cell[i])->text));
	  break;
	default:
	  continue;
	  break;
	}

      switch (hlist->column[i].justification)
	{
	case GTK_JUSTIFY_LEFT:
	  offset = clip_rectangle.x + hlist_row->cell[i].horizontal;
	  break;
	case GTK_JUSTIFY_RIGHT:
	  offset = (clip_rectangle.x + hlist_row->cell[i].horizontal +
		    clip_rectangle.width - width);
	  break;
	case GTK_JUSTIFY_CENTER:
	case GTK_JUSTIFY_FILL:
	  offset = (clip_rectangle.x + hlist_row->cell[i].horizontal +
		    (clip_rectangle.width / 2) - (width / 2));
	  break;
	};

      /* Draw Text and/or Pixmap */
      switch (hlist_row->cell[i].type)
	{
	case GTK_HELL_PIXMAP:
	  if (hlist->stipple_gc && bg_gc == widget->style->bg_gc[GTK_STATE_SELECTED])
	    fill_gc = hlist->stipple_gc;
	  else
	    fill_gc = 0;
	  draw_cell_pixmap (hlist->hlist_window, &clip_rectangle, fg_gc, fill_gc,
			    GTK_HELL_PIXMAP (hlist_row->cell[i])->pixmap,
			    GTK_HELL_PIXMAP (hlist_row->cell[i])->mask,
			    offset,
			    clip_rectangle.y + hlist_row->cell[i].vertical +
			    (clip_rectangle.height - height) / 2,
			    pixmap_width, height);
	  break;
	case GTK_HELL_PIXTEXT:
	  if (hlist->stipple_gc && bg_gc == widget->style->bg_gc[GTK_STATE_SELECTED])
	    fill_gc = hlist->stipple_gc;
	  else
	    fill_gc = 0;
	  draw_cell_pixmap (hlist->hlist_window, &clip_rectangle, fg_gc, fill_gc,
			    GTK_HELL_PIXTEXT (hlist_row->cell[i])->pixmap,
			    GTK_HELL_PIXTEXT (hlist_row->cell[i])->mask,
			    offset,
			    clip_rectangle.y + hlist_row->cell[i].vertical+
			    (clip_rectangle.height - height) / 2,
			    pixmap_width, height);
	  offset += GTK_HELL_PIXTEXT (hlist_row->cell[i])->spacing;
	case GTK_HELL_TEXT:
	  if (style != GTK_WIDGET (hlist)->style)
	    row_center_offset = (((hlist->row_height[row] - style->font->ascent -
				  style->font->descent - 1) / 2) + 1.5 +
				 style->font->ascent);
	  else
	    row_center_offset = hlist->row_center_offset;
	  gdk_gc_set_clip_rectangle (fg_gc, &clip_rectangle);
	  gdk_draw_string (hlist->hlist_window, style->font, fg_gc,
			   offset,
			   row_rectangle.y + row_center_offset + 
			   hlist_row->cell[i].vertical,
			   (hlist_row->cell[i].type == GTK_HELL_PIXTEXT) ?
			   GTK_HELL_PIXTEXT (hlist_row->cell[i])->text :
			   GTK_HELL_TEXT (hlist_row->cell[i])->text);
	  gdk_gc_set_clip_rectangle (fg_gc, NULL);
	  break;
	default:
	  break;
	}
    }

  /* draw focus rectangle */
  if (hlist->focus_row == row &&
      GTK_WIDGET_CAN_FOCUS (widget) && GTK_WIDGET_HAS_FOCUS(widget))
    {
      if (!area)
	gdk_draw_rectangle (hlist->hlist_window, hlist->xor_gc, FALSE,
			    row_rectangle.x, row_rectangle.y,
			    row_rectangle.width - 1, row_rectangle.height - 1);
      else if (gdk_rectangle_intersect (area, &row_rectangle,
					&intersect_rectangle))
	{
	  gdk_gc_set_clip_rectangle (hlist->xor_gc, &intersect_rectangle);
	  gdk_draw_rectangle (hlist->hlist_window, hlist->xor_gc, FALSE,
			      row_rectangle.x, row_rectangle.y,
			      row_rectangle.width - 1,
			      row_rectangle.height - 1);
	  gdk_gc_set_clip_rectangle (hlist->xor_gc, NULL);
	}
    }
}

static int
row_heights_all_zero (GtkHList *hlist)
{
	/* XXX */
	return 0;
}

static void
draw_rows (GtkHList     *hlist,
	   GdkRectangle *area)
{
  GList *list;
  GtkHListRow *hlist_row;
  gint i;
  gint first_row;
  gint last_row;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (row_heights_all_zero(hlist) ||
      !GTK_WIDGET_DRAWABLE (hlist))
    return;

  if (area)
    {
      first_row = ROW_FROM_YPIXEL (hlist, area->y);
      last_row = ROW_FROM_YPIXEL (hlist, area->y + area->height);
    }
  else
    {
      first_row = ROW_FROM_YPIXEL (hlist, 0);
      last_row = ROW_FROM_YPIXEL (hlist, hlist->hlist_window_height);
    }

  /* this is a small special case which exposes the bottom cell line
   * on the last row -- it might go away if I change the wall the cell
   * spacings are drawn
   */
  if (hlist->rows == first_row)
    first_row--;

  list = g_list_nth (hlist->row_list, first_row);
  i = first_row;
  while (list)
    {
      hlist_row = list->data;
      list = list->next;

      if (i > last_row)
	return;
      if (area || hlist_row->draw_me) {
	GTK_HLIST_CLASS_FW (hlist)->draw_row (hlist, area, i, hlist_row);
        hlist_row->draw_me = 0;
      }
      i++;
    }
  if (!area)
    gdk_window_clear_area (hlist->hlist_window, 0,
			   ROW_TOP_YPIXEL (hlist, i), 0, 0);
}

static void                          
draw_xor_line (GtkHList *hlist)
{
  GtkWidget *widget;

  g_return_if_fail (hlist != NULL);

  widget = GTK_WIDGET (hlist);

  gdk_draw_line (widget->window, hlist->xor_gc,
                 hlist->x_drag,
		 widget->style->klass->ythickness,
                 hlist->x_drag,
                 hlist->column_title_area.height +
		 hlist->hlist_window_height + 1);
}

static void
hlist_refresh (GtkHList *hlist)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));
  
  if (HLIST_UNFROZEN (hlist))
    { 
      adjust_adjustments (hlist, FALSE);
      draw_rows (hlist, NULL);
    }
}

/* get cell from coordinates
 *   get_selection_info
 *   gtk_hlist_get_selection_info
 */
static gint
get_selection_info (GtkHList *hlist,
		    gint      x,
		    gint      y,
		    gint     *row,
		    gint     *column)
{
  gint trow, tcol;

  g_return_val_if_fail (hlist != NULL, 0);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), 0);

  /* bounds checking, return false if the user clicked 
   * on a blank area */
  trow = ROW_FROM_YPIXEL (hlist, y);
  if (trow >= hlist->rows)
    return 0;

  if (row)
    *row = trow;

  tcol = COLUMN_FROM_XPIXEL (hlist, x);
  if (tcol >= hlist->columns)
    return 0;

  if (column)
    *column = tcol;

  return 1;
}

gint
gtk_hlist_get_selection_info (GtkHList *hlist, 
			      gint      x, 
			      gint      y, 
			      gint     *row, 
			      gint     *column)
{
  g_return_val_if_fail (hlist != NULL, 0);
  g_return_val_if_fail (GTK_IS_HLIST (hlist), 0);
  return get_selection_info (hlist, x, y, row, column);
}

/* PRIVATE ADJUSTMENT FUNCTIONS
 *   adjust_adjustments
 *   vadjustment_changed
 *   hadjustment_changed
 *   vadjustment_value_changed
 *   hadjustment_value_changed 
 *   check_exposures
 */
static void
adjust_adjustments (GtkHList *hlist,
		    gboolean  block_resize)
{
  if (hlist->vadjustment)
    {
      hlist->vadjustment->page_size = hlist->hlist_window_height;
      hlist->vadjustment->page_increment = hlist->hlist_window_height / 2;
      hlist->vadjustment->step_increment = hlist->row_height[0];
      hlist->vadjustment->lower = 0;
      hlist->vadjustment->upper = LIST_HEIGHT (hlist);

      if (hlist->hlist_window_height - hlist->voffset > LIST_HEIGHT (hlist) ||
	  (hlist->voffset + (gint)hlist->vadjustment->value) != 0)
	{
	  hlist->vadjustment->value = MAX (0, (LIST_HEIGHT (hlist) -
					       hlist->hlist_window_height));
	  gtk_signal_emit_by_name (GTK_OBJECT (hlist->vadjustment),
				   "value_changed");
	}
      gtk_signal_emit_by_name (GTK_OBJECT (hlist->vadjustment), "changed");
    }

  if (hlist->hadjustment)
    {
      hlist->hadjustment->page_size = hlist->hlist_window_width;
      hlist->hadjustment->page_increment = hlist->hlist_window_width / 2;
      hlist->hadjustment->step_increment = 10;
      hlist->hadjustment->lower = 0;
      hlist->hadjustment->upper = LIST_WIDTH (hlist);

      if (hlist->hlist_window_width - hlist->hoffset > LIST_WIDTH (hlist) ||
	  (hlist->hoffset + (gint)hlist->hadjustment->value) != 0)
	{
	  hlist->hadjustment->value = MAX (0, (LIST_WIDTH (hlist) -
					       hlist->hlist_window_width));
	  gtk_signal_emit_by_name (GTK_OBJECT (hlist->hadjustment),
				   "value_changed");
	}
      gtk_signal_emit_by_name (GTK_OBJECT (hlist->hadjustment), "changed");
    }

  if (!block_resize && (!hlist->vadjustment || !hlist->hadjustment))
    {
      GtkWidget *widget;
      GtkRequisition requisition;

      widget = GTK_WIDGET (hlist);
      gtk_widget_size_request (widget, &requisition);

      if ((!hlist->hadjustment &&
	   requisition.width != widget->allocation.width) ||
	  (!hlist->vadjustment &&
	   requisition.height != widget->allocation.height))
	gtk_widget_queue_resize (widget);
    }
}

static void
vadjustment_changed (GtkAdjustment *adjustment,
		     gpointer       data)
{
  GtkHList *hlist;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  hlist = GTK_HLIST (data);
}

static void
hadjustment_changed (GtkAdjustment *adjustment,
		     gpointer       data)
{
  GtkHList *hlist;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  hlist = GTK_HLIST (data);
}

static void
vadjustment_value_changed (GtkAdjustment *adjustment,
			   gpointer       data)
{
  GtkHList *hlist;
  GdkRectangle area;
  gint diff, value;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);
  g_return_if_fail (GTK_IS_HLIST (data));

  hlist = GTK_HLIST (data);

  if (!GTK_WIDGET_DRAWABLE (hlist) || adjustment != hlist->vadjustment)
    return;
  value = adjustment->value;

  if (value > -hlist->voffset)
    {
      /* scroll down */
      diff = value + hlist->voffset;
      if (!diff)
	return;

      /* we have to re-draw the whole screen here... */
      if (diff >= hlist->hlist_window_height)
	{
	  hlist->voffset = -value;
	  draw_rows (hlist, NULL);
	  return;
	}

      if ((diff != 0) && (diff != hlist->hlist_window_height))
	gdk_window_copy_area (hlist->hlist_window, hlist->fg_gc,
			      0, 0, hlist->hlist_window, 0, diff,
			      hlist->hlist_window_width,
			      hlist->hlist_window_height - diff);

      area.x = 0;
      area.y = hlist->hlist_window_height - diff;
      area.width = hlist->hlist_window_width;
      area.height = diff;
    }
  else
    {
      /* scroll up */
      diff = -hlist->voffset - value;
      if (!diff)
	return;

      /* we have to re-draw the whole screen here... */
      if (diff >= hlist->hlist_window_height)
	{
	  hlist->voffset = -value;
	  draw_rows (hlist, NULL);
	  return;
	}

      if ((diff != 0) && (diff != hlist->hlist_window_height))
	gdk_window_copy_area (hlist->hlist_window, hlist->fg_gc,
			      0, diff, hlist->hlist_window, 0, 0,
			      hlist->hlist_window_width,
			      hlist->hlist_window_height - diff);

      area.x = 0;
      area.y = 0;
      area.width = hlist->hlist_window_width;
      area.height = diff;
    }

  hlist->voffset = -value;
  if ((diff != 0) && (diff != hlist->hlist_window_height))
    check_exposures (hlist);

  draw_rows (hlist, &area);
}

static void
hadjustment_value_changed (GtkAdjustment *adjustment,
			   gpointer       data)
{
  GtkHList *hlist;
  GdkRectangle area;
  gint i;
  gint y = 0;
  gint diff = 0;
  gint value;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);
  g_return_if_fail (GTK_IS_HLIST (data));

  hlist = GTK_HLIST (data);

  if (!GTK_WIDGET_DRAWABLE (hlist) || adjustment != hlist->hadjustment)
    return;

  value = adjustment->value;

  /* move the column buttons and resize windows */
  for (i = 0; i < hlist->columns; i++)
    {
      if (hlist->column[i].button)
	{
	  hlist->column[i].button->allocation.x -= value + hlist->hoffset;
	  
	  if (hlist->column[i].button->window)
	    {
	      gdk_window_move (hlist->column[i].button->window,
			       hlist->column[i].button->allocation.x,
			       hlist->column[i].button->allocation.y);
	      
	      if (hlist->column[i].window)
		gdk_window_move (hlist->column[i].window,
				 hlist->column[i].button->allocation.x +
				 hlist->column[i].button->allocation.width - 
				 (DRAG_WIDTH / 2), 0); 
	    }
	}
    }

  if (value > -hlist->hoffset)
    {
      /* scroll right */
      diff = value + hlist->hoffset;
      
      hlist->hoffset = -value;
      
      /* we have to re-draw the whole screen here... */
      if (diff >= hlist->hlist_window_width)
	{
	  draw_rows (hlist, NULL);
	  return;
	}

      if (GTK_WIDGET_CAN_FOCUS(hlist) && GTK_WIDGET_HAS_FOCUS(hlist) &&
	  !GTK_HLIST_CHILD_HAS_FOCUS(hlist) && GTK_HLIST_ADD_MODE(hlist))
	{
	  y = ROW_TOP_YPIXEL (hlist, hlist->focus_row);
	      
	  gdk_draw_rectangle (hlist->hlist_window, hlist->xor_gc, FALSE, 0, y,
			      hlist->hlist_window_width - 1,
			      hlist->row_height[hlist->focus_row] - 1);
	}
      gdk_window_copy_area (hlist->hlist_window,
			    hlist->fg_gc,
			    0, 0,
			    hlist->hlist_window,
			    diff,
			    0,
			    hlist->hlist_window_width - diff,
			    hlist->hlist_window_height);

      area.x = hlist->hlist_window_width - diff;
    }
  else
    {
      /* scroll left */
      if (!(diff = -hlist->hoffset - value))
	return;

      hlist->hoffset = -value;
      
      /* we have to re-draw the whole screen here... */
      if (diff >= hlist->hlist_window_width)
	{
	  draw_rows (hlist, NULL);
	  return;
	}
      
      if (GTK_WIDGET_CAN_FOCUS(hlist) && GTK_WIDGET_HAS_FOCUS(hlist) &&
	  !GTK_HLIST_CHILD_HAS_FOCUS(hlist) && GTK_HLIST_ADD_MODE(hlist))
	{
	  y = ROW_TOP_YPIXEL (hlist, hlist->focus_row);
	  
	  gdk_draw_rectangle (hlist->hlist_window, hlist->xor_gc, FALSE, 0, y,
			      hlist->hlist_window_width - 1,
			      hlist->row_height[hlist->focus_row] - 1);
	}

      gdk_window_copy_area (hlist->hlist_window,
			    hlist->fg_gc,
			    diff, 0,
			    hlist->hlist_window,
			    0,
			    0,
			    hlist->hlist_window_width - diff,
			    hlist->hlist_window_height);
	  
      area.x = 0;
    }

  area.y = 0;
  area.width = diff;
  area.height = hlist->hlist_window_height;

  check_exposures (hlist);

  if (GTK_WIDGET_CAN_FOCUS(hlist) && GTK_WIDGET_HAS_FOCUS(hlist) &&
      !GTK_HLIST_CHILD_HAS_FOCUS(hlist))
    {
      if (GTK_HLIST_ADD_MODE(hlist))
	{
	  gint focus_row;
	  
	  focus_row = hlist->focus_row;
	  hlist->focus_row = -1;
	  draw_rows (hlist, &area);
	  hlist->focus_row = focus_row;
	  
	  gdk_draw_rectangle (hlist->hlist_window, hlist->xor_gc,
			      FALSE, 0, y, hlist->hlist_window_width - 1,
			      hlist->row_height[focus_row] - 1);
	  return;
	}
      else
	{
	  gint x0;
	  gint x1;
	  
	  if (area.x == 0)
	    {
	      x0 = hlist->hlist_window_width - 1;
	      x1 = diff;
	    }
	  else
	    {
	      x0 = 0;
	      x1 = area.x - 1;
	    }
	  
	  y = ROW_TOP_YPIXEL (hlist, hlist->focus_row);
	  gdk_draw_line (hlist->hlist_window, hlist->xor_gc,
			 x0, y + 1, x0, y + hlist->row_height[hlist->focus_row] - 2);
	  gdk_draw_line (hlist->hlist_window, hlist->xor_gc,
			 x1, y + 1, x1, y + hlist->row_height[hlist->focus_row] - 2);
	  
	}
    }
  draw_rows (hlist, &area);
}

static void
check_exposures (GtkHList *hlist)
{
  GdkEvent *event;

  if (!GTK_WIDGET_REALIZED (hlist))
    return;

  /* Make sure graphics expose events are processed before scrolling
   * again */
  while ((event = gdk_event_get_graphics_expose (hlist->hlist_window)) != NULL)
    {
      gtk_widget_event (GTK_WIDGET (hlist), event);
      if (event->expose.count == 0)
	{
	  gdk_event_free (event);
	  break;
	}
      gdk_event_free (event);
    }
}

/* PRIVATE 
 * Memory Allocation/Distruction Routines for GtkHList stuctures
 *
 * functions:
 *   columns_new
 *   column_title_new
 *   columns_delete
 *   row_new
 *   row_delete
 */
static GtkHListColumn *
columns_new (GtkHList *hlist)
{
  GtkHListColumn *column;
  gint i;

  column = g_new (GtkHListColumn, hlist->columns);

  for (i = 0; i < hlist->columns; i++)
    {
      column[i].area.x = 0;
      column[i].area.y = 0;
      column[i].area.width = 0;
      column[i].area.height = 0;
      column[i].title = NULL;
      column[i].button = NULL;
      column[i].window = NULL;
      column[i].width = 0;
      column[i].min_width = -1;
      column[i].max_width = -1;
      column[i].visible = TRUE;
      column[i].width_set = FALSE;
      column[i].resizeable = TRUE;
      column[i].auto_resize = FALSE;
      column[i].button_passive = FALSE;
      column[i].justification = GTK_JUSTIFY_LEFT;
    }

  return column;
}

static void
column_title_new (GtkHList    *hlist,
		  gint         column,
		  const gchar *title)
{
  if (hlist->column[column].title)
    g_free (hlist->column[column].title);

  hlist->column[column].title = g_strdup (title);
}

static void
columns_delete (GtkHList *hlist)
{
  gint i;

  for (i = 0; i < hlist->columns; i++)
    if (hlist->column[i].title)
      g_free (hlist->column[i].title);
      
  g_free (hlist->column);
}

static GtkHListRow *
row_new (GtkHList *hlist)
{
  int i;
  GtkHListRow *hlist_row;

  hlist_row = g_chunk_new (GtkHListRow, hlist->row_mem_chunk);
  hlist_row->cell = g_chunk_new (GtkHell, hlist->cell_mem_chunk);

  for (i = 0; i < hlist->columns; i++)
    {
      hlist_row->cell[i].type = GTK_HELL_EMPTY;
      hlist_row->cell[i].vertical = 0;
      hlist_row->cell[i].horizontal = 0;
      hlist_row->cell[i].style = NULL;
    }

  hlist_row->fg_set = FALSE;
  hlist_row->bg_set = FALSE;
  hlist_row->style = NULL;
  hlist_row->selectable = TRUE;
  hlist_row->state = GTK_STATE_NORMAL;
  hlist_row->data = NULL;
  hlist_row->destroy = NULL;

  return hlist_row;
}

static void
row_delete (GtkHList    *hlist,
	    GtkHListRow *hlist_row)
{
  gint i;

  for (i = 0; i < hlist->columns; i++)
    {
      GTK_HLIST_CLASS_FW (hlist)->set_cell_contents
	(hlist, hlist_row, i, GTK_HELL_EMPTY, NULL, 0, NULL, NULL);
      if (hlist_row->cell[i].style)
	{
	  if (GTK_WIDGET_REALIZED (hlist))
	    gtk_style_detach (hlist_row->cell[i].style);
	  gtk_style_unref (hlist_row->cell[i].style);
	}
    }

  if (hlist_row->style)
    {
      if (GTK_WIDGET_REALIZED (hlist))
        gtk_style_detach (hlist_row->style);
      gtk_style_unref (hlist_row->style);
    }

  if (hlist_row->destroy)
    hlist_row->destroy (hlist_row->data);

  g_mem_chunk_free (hlist->cell_mem_chunk, hlist_row->cell);
  g_mem_chunk_free (hlist->row_mem_chunk, hlist_row);
}

/* FOCUS FUNCTIONS
 *   gtk_hlist_focus
 *   gtk_hlist_draw_focus
 *   gtk_hlist_focus_in
 *   gtk_hlist_focus_out
 *   gtk_hlist_set_focus_child
 *   title_focus
 */
static gint
gtk_hlist_focus (GtkContainer     *container,
		 GtkDirectionType  direction)
{
  GtkHList *hlist;
  GtkWidget *focus_child;
  gint old_row;

  g_return_val_if_fail (container != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HLIST (container), FALSE);

  if (!GTK_WIDGET_IS_SENSITIVE (container))
    return FALSE;
  
  hlist = GTK_HLIST (container);
  focus_child = container->focus_child;
  old_row = hlist->focus_row;

  switch (direction)
    {
    case GTK_DIR_LEFT:
    case GTK_DIR_RIGHT:
      if (GTK_HLIST_CHILD_HAS_FOCUS(hlist))
	{
	  if (title_focus (hlist, direction))
	    return TRUE;
	  gtk_container_set_focus_child (container, NULL);
	  return FALSE;
	 }
      gtk_widget_grab_focus (GTK_WIDGET (container));
      return TRUE;
    case GTK_DIR_DOWN:
    case GTK_DIR_TAB_FORWARD:
      if (GTK_HLIST_CHILD_HAS_FOCUS(hlist))
	{
	  gboolean tf = FALSE;

	  if (((focus_child && direction == GTK_DIR_DOWN) ||
	       !(tf = title_focus (hlist, GTK_DIR_TAB_FORWARD)))
	      && hlist->rows)
	    {
	      if (hlist->focus_row < 0)
		{
		  hlist->focus_row = 0;

		  if ((hlist->selection_mode == GTK_SELECTION_BROWSE ||
		       hlist->selection_mode == GTK_SELECTION_EXTENDED) &&
		      !hlist->selection)
		    gtk_signal_emit (GTK_OBJECT (hlist),
				     hlist_signals[SELECT_ROW],
				     hlist->focus_row, -1, NULL);
		}
	      gtk_widget_grab_focus (GTK_WIDGET (container));
	      return TRUE;
	    }

	  if (tf)
	    return TRUE;
	}
      
      GTK_HLIST_SET_FLAG (hlist, HLIST_CHILD_HAS_FOCUS);
      break;
    case GTK_DIR_UP:
    case GTK_DIR_TAB_BACKWARD:
      if (!focus_child &&
	  GTK_HLIST_CHILD_HAS_FOCUS(hlist) && hlist->rows)
	{
	  if (hlist->focus_row < 0)
	    {
	      hlist->focus_row = 0;
	      if ((hlist->selection_mode == GTK_SELECTION_BROWSE ||
		   hlist->selection_mode == GTK_SELECTION_EXTENDED) &&
		  !hlist->selection)
		gtk_signal_emit (GTK_OBJECT (hlist),
				 hlist_signals[SELECT_ROW],
				 hlist->focus_row, -1, NULL);
	    }
	  gtk_widget_grab_focus (GTK_WIDGET (container));
	  return TRUE;
	}

      GTK_HLIST_SET_FLAG (hlist, HLIST_CHILD_HAS_FOCUS);

      if (title_focus (hlist, direction))
	return TRUE;

      break;
    default:
      break;
    }

  gtk_container_set_focus_child (container, NULL);
  return FALSE;
}

static void
gtk_hlist_draw_focus (GtkWidget *widget)
{
  GtkHList *hlist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HLIST (widget));

  if (!GTK_WIDGET_DRAWABLE (widget) || !GTK_WIDGET_CAN_FOCUS (widget))
    return;

  hlist = GTK_HLIST (widget);
  if (hlist->focus_row >= 0)
    gdk_draw_rectangle (hlist->hlist_window, hlist->xor_gc, FALSE,
			0, ROW_TOP_YPIXEL(hlist, hlist->focus_row),
			hlist->hlist_window_width - 1,
			hlist->row_height[hlist->focus_row] - 1);
}

static gint
gtk_hlist_focus_in (GtkWidget     *widget,
		    GdkEventFocus *event)
{
  GtkHList *hlist;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
  GTK_HLIST_UNSET_FLAG (widget, HLIST_CHILD_HAS_FOCUS);

  hlist = GTK_HLIST (widget);

  if (hlist->selection_mode == GTK_SELECTION_BROWSE &&
      hlist->selection == NULL && hlist->focus_row > -1)
    {
      GList *list;

      list = g_list_nth (hlist->row_list, hlist->focus_row);
      if (list && GTK_HLIST_ROW (list)->selectable)
	gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[SELECT_ROW],
			 hlist->focus_row, -1, event);
      else
	gtk_widget_draw_focus (widget);
    }
  else
    gtk_widget_draw_focus (widget);

  return FALSE;
}

static gint
gtk_hlist_focus_out (GtkWidget     *widget,
		     GdkEventFocus *event)
{
  GtkHList *hlist;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HLIST (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
  GTK_HLIST_SET_FLAG (widget, HLIST_CHILD_HAS_FOCUS);

  gtk_widget_draw_focus (widget);
  
  hlist = GTK_HLIST (widget);

  GTK_HLIST_CLASS_FW (widget)->resync_selection (hlist, (GdkEvent *) event);

  return FALSE;
}

static void
gtk_hlist_set_focus_child (GtkContainer *container,
			   GtkWidget    *child)
{
  g_return_if_fail (container != NULL);
  g_return_if_fail (GTK_IS_HLIST (container));

  if (child)
    {
      g_return_if_fail (GTK_IS_WIDGET (child));
      GTK_HLIST_SET_FLAG (container, HLIST_CHILD_HAS_FOCUS);
    }

  parent_class->set_focus_child (container, child);
}

static gboolean
title_focus (GtkHList *hlist,
	     gint      dir)
{
  GtkWidget *focus_child;
  gboolean return_val = FALSE;
  gint last_column;
  gint d = 1;
  gint i = 0;
  gint j;

  if (!GTK_HLIST_SHOW_TITLES(hlist))
    return FALSE;

  focus_child = GTK_CONTAINER (hlist)->focus_child;

  for (last_column = hlist->columns - 1;
       last_column >= 0 && !hlist->column[last_column].visible; last_column--)
    ;
  
  switch (dir)
    {
    case GTK_DIR_TAB_BACKWARD:
    case GTK_DIR_UP:
      if (!focus_child || !GTK_HLIST_CHILD_HAS_FOCUS(hlist))
	{
	  if (dir == GTK_DIR_UP)
	    i = COLUMN_FROM_XPIXEL (hlist, 0);
	  else
	    i = last_column;
	  focus_child = hlist->column[i].button;
	  dir = GTK_DIR_TAB_FORWARD;
	}
      else
	d = -1;
      break;
    case GTK_DIR_LEFT:
      d = -1;
      if (!focus_child)
	{
	  i = last_column;
	  focus_child = hlist->column[i].button;
	}
      break;
    case GTK_DIR_RIGHT:
      if (!focus_child)
	{
	  i = 0;
	  focus_child = hlist->column[i].button;
	}
      break;
    }

  if (focus_child)
    while (i < hlist->columns)
      {
	if (hlist->column[i].button == focus_child)
	  {
	    if (hlist->column[i].button && 
		GTK_WIDGET_VISIBLE (hlist->column[i].button) &&
		GTK_IS_CONTAINER (hlist->column[i].button) &&
		!GTK_WIDGET_HAS_FOCUS(hlist->column[i].button))
	      if (gtk_container_focus 
		  (GTK_CONTAINER (hlist->column[i].button), dir))
		{
		  return_val = TRUE;
		  i -= d;
		}
	    if (!return_val && dir == GTK_DIR_UP)
	      return FALSE;
	    i += d;
	    break;
	  }
	i++;
      }

  j = i;

  if (!return_val)
    while (j >= 0 && j < hlist->columns)
      {
	if (hlist->column[j].button &&
	    GTK_WIDGET_VISIBLE (hlist->column[j].button))
	  {
	    if (GTK_IS_CONTAINER (hlist->column[j].button) &&
		gtk_container_focus 
		(GTK_CONTAINER (hlist->column[j].button), dir))
	      {
		return_val = TRUE;
		break;
	      }
	    else if (GTK_WIDGET_CAN_FOCUS (hlist->column[j].button))
	      {
		gtk_widget_grab_focus (hlist->column[j].button);
		return_val = TRUE;
		break;
	      }
	  }
	j += d;
      }
  
  if (return_val)
    {
      if (COLUMN_LEFT_XPIXEL (hlist, j) < HELL_SPACING + COLUMN_INSET)
	gtk_hlist_moveto (hlist, -1, j, 0, 0);
      else if (COLUMN_LEFT_XPIXEL(hlist, j) + hlist->column[j].area.width >
	       hlist->hlist_window_width)
	{
	  if (j == last_column)
	    gtk_hlist_moveto (hlist, -1, j, 0, 0);
	  else
	    gtk_hlist_moveto (hlist, -1, j, 0, 1);
	}
    }
  return return_val;
}

/* PRIVATE SCROLLING FUNCTIONS
 *   move_focus_row
 *   scroll_horizontal
 *   scroll_vertical
 *   move_horizontal
 *   move_vertical
 *   horizontal_timeout
 *   vertical_timeout
 *   remove_grab
 */
static void
move_focus_row (GtkHList      *hlist,
		GtkScrollType  scroll_type,
		gfloat         position)
{
  GtkWidget *widget;

  g_return_if_fail (hlist != 0);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  widget = GTK_WIDGET (hlist);

  switch (scroll_type)
    {
    case GTK_SCROLL_STEP_BACKWARD:
      if (hlist->focus_row <= 0)
	return;
      gtk_hlist_draw_focus (widget);
      hlist->focus_row--;
      gtk_hlist_draw_focus (widget);
      break;
    case GTK_SCROLL_STEP_FORWARD:
      if (hlist->focus_row >= hlist->rows - 1)
	return;
      gtk_hlist_draw_focus (widget);
      hlist->focus_row++;
      gtk_hlist_draw_focus (widget);
      break;
    case GTK_SCROLL_PAGE_BACKWARD:
      if (hlist->focus_row <= 0)
	return;
      gtk_hlist_draw_focus (widget);
      hlist->focus_row = MAX (0, hlist->focus_row -
			      (2 * hlist->hlist_window_height -
			       hlist->row_height[hlist->focus_row] - HELL_SPACING) / 
			      (2 * (hlist->row_height[hlist->focus_row] + HELL_SPACING)));
      gtk_hlist_draw_focus (widget);
      break;
    case GTK_SCROLL_PAGE_FORWARD:
      if (hlist->focus_row >= hlist->rows - 1)
	return;
      gtk_hlist_draw_focus (widget);
      hlist->focus_row = MIN (hlist->rows - 1, hlist->focus_row + 
			      (2 * hlist->hlist_window_height -
			       hlist->row_height[hlist->focus_row] - HELL_SPACING) / 
			      (2 * (hlist->row_height[hlist->focus_row] + HELL_SPACING)));
      gtk_hlist_draw_focus (widget);
      break;
    case GTK_SCROLL_JUMP:
      if (position >= 0 && position <= 1)
	{
	  gtk_hlist_draw_focus (widget);
	  hlist->focus_row = position * (hlist->rows - 1);
	  gtk_hlist_draw_focus (widget);
	}
      break;
    default:
      break;
    }
}

static void
scroll_horizontal (GtkHList      *hlist,
		   GtkScrollType  scroll_type,
		   gfloat         position)
{
  gint column = 0;
  gint last_column;

  g_return_if_fail (hlist != 0);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (hlist))
    return;

  for (last_column = hlist->columns - 1;
       last_column >= 0 && !hlist->column[last_column].visible; last_column--)
    ;

  switch (scroll_type)
    {
    case GTK_SCROLL_STEP_BACKWARD:
      column = COLUMN_FROM_XPIXEL (hlist, 0);
      if (COLUMN_LEFT_XPIXEL (hlist, column) - HELL_SPACING - COLUMN_INSET >= 0
	  && column > 0)
	column--;
      break;
    case GTK_SCROLL_STEP_FORWARD:
      column = COLUMN_FROM_XPIXEL (hlist, hlist->hlist_window_width);
      if (column < 0)
	return;
      if (COLUMN_LEFT_XPIXEL (hlist, column) +
	  hlist->column[column].area.width +
	  HELL_SPACING + COLUMN_INSET - 1 <= hlist->hlist_window_width &&
	  column < last_column)
	column++;
      break;
    case GTK_SCROLL_PAGE_BACKWARD:
    case GTK_SCROLL_PAGE_FORWARD:
      return;
    case GTK_SCROLL_JUMP:
      if (position >= 0 && position <= 1)
	{
	  gint vis_columns = 0;
	  gint i;

	  for (i = 0; i <= last_column; i++)
 	    if (hlist->column[i].visible)
	      vis_columns++;

	  column = position * vis_columns;

	  for (i = 0; i <= last_column && column > 0; i++)
	    if (hlist->column[i].visible)
	      column--;

	  column = i;
	}
      else
	return;
      break;
    default:
      break;
    }

  if (COLUMN_LEFT_XPIXEL (hlist, column) < HELL_SPACING + COLUMN_INSET)
    gtk_hlist_moveto (hlist, -1, column, 0, 0);
  else if (COLUMN_LEFT_XPIXEL (hlist, column) + HELL_SPACING + COLUMN_INSET - 1
	   + hlist->column[column].area.width > hlist->hlist_window_width)
    {
      if (column == last_column)
	gtk_hlist_moveto (hlist, -1, column, 0, 0);
      else
	gtk_hlist_moveto (hlist, -1, column, 0, 1);
    }
}

static void
scroll_vertical (GtkHList      *hlist,
		 GtkScrollType  scroll_type,
		 gfloat         position)
{
  gint old_focus_row;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (hlist))
    return;

  switch (hlist->selection_mode)
    {
    case GTK_SELECTION_EXTENDED:
      if (hlist->anchor >= 0)
	return;
    case GTK_SELECTION_BROWSE:

      old_focus_row = hlist->focus_row;
      move_focus_row (hlist, scroll_type, position);

      if (old_focus_row != hlist->focus_row)
	{
	  if (hlist->selection_mode == GTK_SELECTION_BROWSE)
	    gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[UNSELECT_ROW],
			     old_focus_row, -1, NULL);
	  else if (!GTK_HLIST_ADD_MODE(hlist))
	    {
	      gtk_hlist_unselect_all (hlist);
	      hlist->undo_anchor = old_focus_row;
	    }
	}

      switch (gtk_hlist_row_is_visible (hlist, hlist->focus_row))
	{
	case GTK_VISIBILITY_NONE:
	  if (old_focus_row != hlist->focus_row &&
	      !(hlist->selection_mode == GTK_SELECTION_EXTENDED &&
		GTK_HLIST_ADD_MODE(hlist)))
	    gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[SELECT_ROW],
			     hlist->focus_row, -1, NULL);
	  switch (scroll_type)
	    {
	    case GTK_SCROLL_STEP_BACKWARD:
	    case GTK_SCROLL_PAGE_BACKWARD:
	      gtk_hlist_moveto (hlist, hlist->focus_row, -1, 0, 0);
	      break;
	    case GTK_SCROLL_STEP_FORWARD:
	    case GTK_SCROLL_PAGE_FORWARD:
	      gtk_hlist_moveto (hlist, hlist->focus_row, -1, 1, 0);
	      break;
	    case GTK_SCROLL_JUMP:
	      gtk_hlist_moveto (hlist, hlist->focus_row, -1, 0.5, 0);
	      break;
	    default:
	      break;
	    }
	  break;
	case GTK_VISIBILITY_PARTIAL:
	  switch (scroll_type)
	    {
	    case GTK_SCROLL_STEP_BACKWARD:
	    case GTK_SCROLL_PAGE_BACKWARD:
	      gtk_hlist_moveto (hlist, hlist->focus_row, -1, 0, 0);
	      break;
	    case GTK_SCROLL_STEP_FORWARD:
	    case GTK_SCROLL_PAGE_FORWARD:
	      gtk_hlist_moveto (hlist, hlist->focus_row, -1, 1, 0);
	      break;
	    case GTK_SCROLL_JUMP:
	      gtk_hlist_moveto (hlist, hlist->focus_row, -1, 0.5, 0);
	      break;
	    default:
	      break;
	    }
	default:
	  if (old_focus_row != hlist->focus_row &&
	      !(hlist->selection_mode == GTK_SELECTION_EXTENDED &&
		GTK_HLIST_ADD_MODE(hlist)))
	    gtk_signal_emit (GTK_OBJECT (hlist), hlist_signals[SELECT_ROW],
			     hlist->focus_row, -1, NULL);
	  break;
	}
      break;
    default:
      move_focus_row (hlist, scroll_type, position);

      if (ROW_TOP_YPIXEL (hlist, hlist->focus_row) + hlist->row_height[hlist->focus_row] >
	  hlist->hlist_window_height)
	gtk_hlist_moveto (hlist, hlist->focus_row, -1, 1, 0);
      else if (ROW_TOP_YPIXEL (hlist, hlist->focus_row) < 0)
	gtk_hlist_moveto (hlist, hlist->focus_row, -1, 0, 0);
      break;
    }
}

static void
move_horizontal (GtkHList *hlist,
		 gint      diff)
{
  gfloat value;

  if (!hlist->hadjustment)
    return;

  value = CLAMP (hlist->hadjustment->value + diff, 0.0,
		 hlist->hadjustment->upper - hlist->hadjustment->page_size);
  gtk_adjustment_set_value(hlist->hadjustment, value);
}

static void
move_vertical (GtkHList *hlist,
	       gint      row,
	       gfloat    align)
{
  gfloat value;

  if (!hlist->vadjustment)
    return;

  value = (ROW_TOP_YPIXEL (hlist, row) - hlist->voffset -
	   align * (hlist->hlist_window_height - hlist->row_height[row]) +
	   (2 * align - 1) * HELL_SPACING);

  if (value + hlist->vadjustment->page_size > hlist->vadjustment->upper)
    value = hlist->vadjustment->upper - hlist->vadjustment->page_size;

  gtk_adjustment_set_value(hlist->vadjustment, value);
}

static gint
horizontal_timeout (GtkHList *hlist)
{
  GdkEventMotion event;

  GDK_THREADS_ENTER ();

  hlist->htimer = 0;

  event.type = GDK_MOTION_NOTIFY;
  event.send_event = TRUE;

  gtk_hlist_motion (GTK_WIDGET (hlist), &event);

  GDK_THREADS_LEAVE ();
  
  return FALSE;
}

static gint
vertical_timeout (GtkHList *hlist)
{
  GdkEventMotion event;

  GDK_THREADS_ENTER ();

  hlist->vtimer = 0;

  event.type = GDK_MOTION_NOTIFY;
  event.send_event = TRUE;

  gtk_hlist_motion (GTK_WIDGET (hlist), &event);

  GDK_THREADS_LEAVE ();

  return FALSE;
}

static void
remove_grab (GtkHList *hlist)
{
  if (GTK_WIDGET_HAS_GRAB (hlist))
    {
      gtk_grab_remove (GTK_WIDGET (hlist));
      if (gdk_pointer_is_grabbed ())
	gdk_pointer_ungrab (GDK_CURRENT_TIME);
    }

  if (hlist->htimer)
    {
      gtk_timeout_remove (hlist->htimer);
      hlist->htimer = 0;
    }

  if (hlist->vtimer)
    {
      gtk_timeout_remove (hlist->vtimer);
      hlist->vtimer = 0;
    }
}

/* PUBLIC SORTING FUNCTIONS
 * gtk_hlist_sort
 * gtk_hlist_set_compare_func
 * gtk_hlist_set_auto_sort
 * gtk_hlist_set_sort_type
 * gtk_hlist_set_sort_column
 */
void
gtk_hlist_sort (GtkHList *hlist)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  GTK_HLIST_CLASS_FW (hlist)->sort_list (hlist);
}

void
gtk_hlist_set_compare_func (GtkHList            *hlist,
			    GtkHListCompareFunc  cmp_func)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  hlist->compare = (cmp_func) ? cmp_func : default_compare;
}

void       
gtk_hlist_set_auto_sort (GtkHList *hlist,
			 gboolean  auto_sort)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));
  
  if (GTK_HLIST_AUTO_SORT(hlist) && !auto_sort)
    GTK_HLIST_UNSET_FLAG (hlist, HLIST_AUTO_SORT);
  else if (!GTK_HLIST_AUTO_SORT(hlist) && auto_sort)
    {
      GTK_HLIST_SET_FLAG (hlist, HLIST_AUTO_SORT);
      gtk_hlist_sort (hlist);
    }
}

void       
gtk_hlist_set_sort_type (GtkHList    *hlist,
			 GtkSortType  sort_type)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));
  
  hlist->sort_type = sort_type;
}

void
gtk_hlist_set_sort_column (GtkHList *hlist,
			   gint      column)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (column < 0 || column >= hlist->columns)
    return;

  hlist->sort_column = column;
}

/* PRIVATE SORTING FUNCTIONS
 *   default_compare
 *   real_sort_list
 *   gtk_hlist_merge
 *   gtk_hlist_mergesort
 */
static gint
default_compare (GtkHList      *hlist,
		 gconstpointer  ptr1,
		 gconstpointer  ptr2)
{
  char *text1 = NULL;
  char *text2 = NULL;

  GtkHListRow *row1 = (GtkHListRow *) ptr1;
  GtkHListRow *row2 = (GtkHListRow *) ptr2;

  switch (row1->cell[hlist->sort_column].type)
    {
    case GTK_HELL_TEXT:
      text1 = GTK_HELL_TEXT (row1->cell[hlist->sort_column])->text;
      break;
    case GTK_HELL_PIXTEXT:
      text1 = GTK_HELL_PIXTEXT (row1->cell[hlist->sort_column])->text;
      break;
    default:
      break;
    }
 
  switch (row2->cell[hlist->sort_column].type)
    {
    case GTK_HELL_TEXT:
      text2 = GTK_HELL_TEXT (row2->cell[hlist->sort_column])->text;
      break;
    case GTK_HELL_PIXTEXT:
      text2 = GTK_HELL_PIXTEXT (row2->cell[hlist->sort_column])->text;
      break;
    default:
      break;
    }

  if (!text2)
    return (text1 != NULL);

  if (!text1)
    return -1;

  return strcmp (text1, text2);
}

static void
real_sort_list (GtkHList *hlist)
{
  GList *list;
  GList *work;
  gint i;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (hlist->rows <= 1)
    return;

  if (gdk_pointer_is_grabbed () && GTK_WIDGET_HAS_GRAB (hlist))
    return;

  gtk_hlist_freeze (hlist);

  if (hlist->anchor != -1 && hlist->selection_mode == GTK_SELECTION_EXTENDED)
    {
      GTK_HLIST_CLASS_FW (hlist)->resync_selection (hlist, NULL);
      g_list_free (hlist->undo_selection);
      g_list_free (hlist->undo_unselection);
      hlist->undo_selection = NULL;
      hlist->undo_unselection = NULL;
    }
   
  hlist->row_list = gtk_hlist_mergesort (hlist, hlist->row_list, hlist->rows);

  work = hlist->selection;

  for (i = 0, list = hlist->row_list; i < hlist->rows; i++, list = list->next)
    {
      if (GTK_HLIST_ROW (list)->state == GTK_STATE_SELECTED)
	{
	  work->data = GINT_TO_POINTER (i);
	  work = work->next;
	}
      
      if (i == hlist->rows - 1)
	hlist->row_list_end = list;
    }

  gtk_hlist_thaw (hlist);
}

static GList *
gtk_hlist_merge (GtkHList *hlist,
		 GList    *a,         /* first list to merge */
		 GList    *b)         /* second list to merge */
{
  GList z = { 0, 0, 0 };              /* auxiliary node */
  GList *c;
  gint cmp;

  c = &z;

  while (a || b)
    {
      if (a && !b)
	{
	  c->next = a;
	  a->prev = c;
	  c = a;
	  a = a->next;
	  break;
	}
      else if (!a && b)
	{
	  c->next = b;
	  b->prev = c;
	  c = b;
	  b = b->next;
	  break;
	}
      else /* a && b */
	{
	  cmp = hlist->compare (hlist, GTK_HLIST_ROW (a), GTK_HLIST_ROW (b));
	  if ((cmp >= 0 && hlist->sort_type == GTK_SORT_DESCENDING) ||
	      (cmp <= 0 && hlist->sort_type == GTK_SORT_ASCENDING) ||
	      (a && !b))
	    {
	      c->next = a;
	      a->prev = c;
	      c = a;
	      a = a->next;
	    }
	  else
	    {
	      c->next = b;
	      b->prev = c;
	      c = b;
	      b = b->next;
	    }
	}
    }

  return z.next;
}

static GList *
gtk_hlist_mergesort (GtkHList *hlist,
		     GList    *list,         /* the list to sort */
		     gint      num)          /* the list's length */
{
  GList *half;
  gint i;

  if (num == 1)
    {
      return list;
    }
  else
    {
      /* move "half" to the middle */
      half = list;
      for (i = 0; i < num / 2; i++)
	half = half->next;

      /* cut the list in two */
      half->prev->next = NULL;
      half->prev = NULL;

      /* recursively sort both lists */
      return gtk_hlist_merge (hlist,
		       gtk_hlist_mergesort (hlist, list, num / 2),
		       gtk_hlist_mergesort (hlist, half, num - num / 2));
    }
}

/************************/

static void
drag_source_info_destroy (gpointer data)
{
  GtkHListHellInfo *info = data;

  g_free (info);
}

static void
drag_dest_info_destroy (gpointer data)
{
  GtkHListDestInfo *info = data;

  g_free (info);
}

static void
drag_dest_cell (GtkHList         *hlist,
		gint              x,
		gint              y,
		GtkHListDestInfo *dest_info)
{
  GtkWidget *widget;

  widget = GTK_WIDGET (hlist);

  dest_info->insert_pos = GTK_HLIST_DRAG_NONE;

  y -= (GTK_CONTAINER (hlist)->border_width +
	widget->style->klass->ythickness +
	hlist->column_title_area.height);

  dest_info->cell.row = ROW_FROM_YPIXEL (hlist, y);
  if (dest_info->cell.row >= hlist->rows)
    {
      dest_info->cell.row = hlist->rows - 1;
      y = ROW_TOP_YPIXEL (hlist, dest_info->cell.row) + hlist->row_height[dest_info->cell.row];
    }
  if (dest_info->cell.row < -1)
    dest_info->cell.row = -1;

  x -= GTK_CONTAINER (widget)->border_width + widget->style->klass->xthickness;
  dest_info->cell.column = COLUMN_FROM_XPIXEL (hlist, x);

  if (dest_info->cell.row >= 0)
    {
      gint y_delta;
      gint h = 0;

      y_delta = y - ROW_TOP_YPIXEL (hlist, dest_info->cell.row);
      
      if (GTK_HLIST_DRAW_DRAG_RECT(hlist))
	{
	  dest_info->insert_pos = GTK_HLIST_DRAG_INTO;
	  h = hlist->row_height[dest_info->cell.row] / 4;
	}
      else if (GTK_HLIST_DRAW_DRAG_LINE(hlist))
	{
	  dest_info->insert_pos = GTK_HLIST_DRAG_BEFORE;
	  h = hlist->row_height[dest_info->cell.row] / 2;
	}

      if (GTK_HLIST_DRAW_DRAG_LINE(hlist))
	{
	  if (y_delta < h)
	    dest_info->insert_pos = GTK_HLIST_DRAG_BEFORE;
	  else if (hlist->row_height[dest_info->cell.row] - y_delta < h)
	    dest_info->insert_pos = GTK_HLIST_DRAG_AFTER;
	}
    }
}

static void
gtk_hlist_drag_begin (GtkWidget	     *widget,
		      GdkDragContext *context)
{
  GtkHList *hlist;
  GtkHListHellInfo *info;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HLIST (widget));
  g_return_if_fail (context != NULL);

  hlist = GTK_HLIST (widget);

  hlist->drag_button = 0;
  remove_grab (hlist);

  switch (hlist->selection_mode)
    {
    case GTK_SELECTION_EXTENDED:
      update_extended_selection (hlist, hlist->focus_row);
      GTK_HLIST_CLASS_FW (hlist)->resync_selection (hlist, NULL);
      break;
    case GTK_SELECTION_SINGLE:
    case GTK_SELECTION_MULTIPLE:
      hlist->anchor = -1;
    case GTK_SELECTION_BROWSE:
      break;
    }

  info = g_dataset_get_data (context, "gtk-hlist-drag-source");

  if (!info)
    {
      info = g_new (GtkHListHellInfo, 1);

      if (hlist->click_cell.row < 0)
	hlist->click_cell.row = 0;
      else if (hlist->click_cell.row >= hlist->rows)
	hlist->click_cell.row = hlist->rows - 1;
      info->row = hlist->click_cell.row;
      info->column = hlist->click_cell.column;

      g_dataset_set_data_full (context, "gtk-hlist-drag-source", info,
			       drag_source_info_destroy);
    }

  if (GTK_HLIST_USE_DRAG_ICONS (hlist))
    gtk_drag_set_icon_default (context);
}

static void
gtk_hlist_drag_end (GtkWidget	   *widget,
		    GdkDragContext *context)
{
  GtkHList *hlist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HLIST (widget));
  g_return_if_fail (context != NULL);

  hlist = GTK_HLIST (widget);

  hlist->click_cell.row = -1;
  hlist->click_cell.column = -1;
}

static void
gtk_hlist_drag_leave (GtkWidget      *widget,
		      GdkDragContext *context,
		      guint           time)
{
  GtkHList *hlist;
  GtkHListDestInfo *dest_info;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HLIST (widget));
  g_return_if_fail (context != NULL);

  hlist = GTK_HLIST (widget);

  dest_info = g_dataset_get_data (context, "gtk-hlist-drag-dest");
  
  if (dest_info)
    {
      if (dest_info->cell.row >= 0 &&
	  GTK_HLIST_REORDERABLE(hlist) &&
	  gtk_drag_get_source_widget (context) == widget)
	{
	  GList *list;
	  GdkAtom atom = gdk_atom_intern ("gtk-hlist-drag-reorder", FALSE);

	  list = context->targets;
	  while (list)
	    {
	      if (atom == (GdkAtom)GPOINTER_TO_INT (list->data))
		{
		  GTK_HLIST_CLASS_FW (hlist)->draw_drag_highlight
		    (hlist,
		     g_list_nth (hlist->row_list, dest_info->cell.row)->data,
		     dest_info->cell.row, dest_info->insert_pos);
		  break;
		}
	      list = list->next;
	    }
	}
      g_dataset_remove_data (context, "gtk-hlist-drag-dest");
    }
}

static gint
gtk_hlist_drag_motion (GtkWidget      *widget,
		       GdkDragContext *context,
		       gint            x,
		       gint            y,
		       guint           time)
{
  GtkHList *hlist;
  GtkHListDestInfo new_info;
  GtkHListDestInfo *dest_info;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HLIST (widget), FALSE);

  hlist = GTK_HLIST (widget);

  dest_info = g_dataset_get_data (context, "gtk-hlist-drag-dest");

  if (!dest_info)
    {
      dest_info = g_new (GtkHListDestInfo, 1);

      dest_info->insert_pos  = GTK_HLIST_DRAG_NONE;
      dest_info->cell.row    = -1;
      dest_info->cell.column = -1;

      g_dataset_set_data_full (context, "gtk-hlist-drag-dest", dest_info,
			       drag_dest_info_destroy);
    }

  drag_dest_cell (hlist, x, y, &new_info);

  if (GTK_HLIST_REORDERABLE (hlist))
    {
      GList *list;
      GdkAtom atom = gdk_atom_intern ("gtk-hlist-drag-reorder", FALSE);

      list = context->targets;
      while (list)
	{
	  if (atom == (GdkAtom)GPOINTER_TO_INT (list->data))
	    break;
	  list = list->next;
	}

      if (list)
	{
	  if (gtk_drag_get_source_widget (context) != widget ||
	      new_info.insert_pos == GTK_HLIST_DRAG_NONE ||
	      new_info.cell.row == hlist->click_cell.row ||
	      (new_info.cell.row == hlist->click_cell.row - 1 &&
	       new_info.insert_pos == GTK_HLIST_DRAG_AFTER) ||
	      (new_info.cell.row == hlist->click_cell.row + 1 &&
	       new_info.insert_pos == GTK_HLIST_DRAG_BEFORE))
	    {
	      if (dest_info->cell.row < 0)
		{
		  gdk_drag_status (context, GDK_ACTION_DEFAULT, time);
		  return FALSE;
		}
	      return TRUE;
	    }
		
	  if (new_info.cell.row != dest_info->cell.row ||
	      (new_info.cell.row == dest_info->cell.row &&
	       dest_info->insert_pos != new_info.insert_pos))
	    {
	      if (dest_info->cell.row >= 0)
		GTK_HLIST_CLASS_FW (hlist)->draw_drag_highlight
		  (hlist, g_list_nth (hlist->row_list,
				      dest_info->cell.row)->data,
		   dest_info->cell.row, dest_info->insert_pos);

	      dest_info->insert_pos  = new_info.insert_pos;
	      dest_info->cell.row    = new_info.cell.row;
	      dest_info->cell.column = new_info.cell.column;
	      
	      GTK_HLIST_CLASS_FW (hlist)->draw_drag_highlight
		(hlist, g_list_nth (hlist->row_list,
				    dest_info->cell.row)->data,
		 dest_info->cell.row, dest_info->insert_pos);

	      gdk_drag_status (context, context->suggested_action, time);
	    }
	  return TRUE;
	}
    }

  dest_info->insert_pos  = new_info.insert_pos;
  dest_info->cell.row    = new_info.cell.row;
  dest_info->cell.column = new_info.cell.column;
  return TRUE;
}

static gboolean
gtk_hlist_drag_drop (GtkWidget      *widget,
		     GdkDragContext *context,
		     gint            x,
		     gint            y,
		     guint           time)
{
  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_HLIST (widget), FALSE);
  g_return_val_if_fail (context != NULL, FALSE);

  if (GTK_HLIST_REORDERABLE (widget) &&
      gtk_drag_get_source_widget (context) == widget)
    {
      GList *list;
      GdkAtom atom = gdk_atom_intern ("gtk-hlist-drag-reorder", FALSE);

      list = context->targets;
      while (list)
	{
	  if (atom == (GdkAtom)GPOINTER_TO_INT (list->data))
	    return TRUE;
	  list = list->next;
	}
    }
  return FALSE;
}

static void
gtk_hlist_drag_data_received (GtkWidget        *widget,
			      GdkDragContext   *context,
			      gint              x,
			      gint              y,
			      GtkSelectionData *selection_data,
			      guint             info,
			      guint             time)
{
  GtkHList *hlist;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HLIST (widget));
  g_return_if_fail (context != NULL);
  g_return_if_fail (selection_data != NULL);

  hlist = GTK_HLIST (widget);

  if (GTK_HLIST_REORDERABLE (hlist) &&
      gtk_drag_get_source_widget (context) == widget &&
      selection_data->target ==
      gdk_atom_intern ("gtk-hlist-drag-reorder", FALSE) &&
      selection_data->format == GTK_TYPE_POINTER &&
      selection_data->length == sizeof (GtkHListHellInfo))
    {
      GtkHListHellInfo *source_info;

      source_info = (GtkHListHellInfo *)(selection_data->data);
      if (source_info)
	{
	  GtkHListDestInfo dest_info;

	  drag_dest_cell (hlist, x, y, &dest_info);

	  if (dest_info.insert_pos == GTK_HLIST_DRAG_AFTER)
	    dest_info.cell.row++;
	  if (source_info->row < dest_info.cell.row)
	    dest_info.cell.row--;
	  if (dest_info.cell.row != source_info->row)
	    gtk_hlist_row_move (hlist, source_info->row, dest_info.cell.row);

	  g_dataset_remove_data (context, "gtk-hlist-drag-dest");
	}
    }
}

static void  
gtk_hlist_drag_data_get (GtkWidget        *widget,
			 GdkDragContext   *context,
			 GtkSelectionData *selection_data,
			 guint             info,
			 guint             time)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_HLIST (widget));
  g_return_if_fail (context != NULL);
  g_return_if_fail (selection_data != NULL);

  if (selection_data->target ==
      gdk_atom_intern ("gtk-hlist-drag-reorder", FALSE))
    {
      GtkHListHellInfo *info;

      info = g_dataset_get_data (context, "gtk-hlist-drag-source");

      if (info)
	{
	  GtkHListHellInfo ret_info;

	  ret_info.row = info->row;
	  ret_info.column = info->column;

	  gtk_selection_data_set (selection_data, selection_data->target,
				  GTK_TYPE_POINTER, (guchar *) &ret_info,
				  sizeof (GtkHListHellInfo));
	}
      else
	gtk_selection_data_set (selection_data, selection_data->target,
				GTK_TYPE_POINTER, NULL,	0);
    }
}

static void
draw_drag_highlight (GtkHList        *hlist,
		     GtkHListRow     *dest_row,
		     gint             dest_row_number,
		     GtkHListDragPos  drag_pos)
{
  gint y;

  y = ROW_TOP_YPIXEL (hlist, dest_row_number) - 1;

  switch (drag_pos)
    {
    case GTK_HLIST_DRAG_NONE:
      break;
    case GTK_HLIST_DRAG_AFTER:
      y += hlist->row_height[dest_row_number] + 1;
    case GTK_HLIST_DRAG_BEFORE:
      gdk_draw_line (hlist->hlist_window, hlist->xor_gc,
		     0, y, hlist->hlist_window_width, y);
      break;
    case GTK_HLIST_DRAG_INTO:
      gdk_draw_rectangle (hlist->hlist_window, hlist->xor_gc, FALSE, 0, y,
			  hlist->hlist_window_width - 1, hlist->row_height[dest_row_number]);
      break;
    }
}

void
gtk_hlist_set_reorderable (GtkHList *hlist, 
			   gboolean  reorderable)
{
  GtkWidget *widget;

  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if ((GTK_HLIST_REORDERABLE(hlist) != 0) == reorderable)
    return;

  widget = GTK_WIDGET (hlist);

  if (reorderable)
    {
      GTK_HLIST_SET_FLAG (hlist, HLIST_REORDERABLE);
      gtk_drag_dest_set (widget,
			 GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
			 &hlist_target_table, 1, GDK_ACTION_MOVE);
    }
  else
    {
      GTK_HLIST_UNSET_FLAG (hlist, HLIST_REORDERABLE);
      gtk_drag_dest_unset (GTK_WIDGET (hlist));
    }
}

void
gtk_hlist_set_use_drag_icons (GtkHList *hlist,
			      gboolean  use_icons)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));

  if (use_icons != 0)
    GTK_HLIST_SET_FLAG (hlist, HLIST_USE_DRAG_ICONS);
  else
    GTK_HLIST_UNSET_FLAG (hlist, HLIST_USE_DRAG_ICONS);
}

void
gtk_hlist_set_button_actions (GtkHList *hlist,
			      guint     button,
			      guint8    button_actions)
{
  g_return_if_fail (hlist != NULL);
  g_return_if_fail (GTK_IS_HLIST (hlist));
  
  if (button < MAX_BUTTON)
    {
      if (gdk_pointer_is_grabbed () || GTK_WIDGET_HAS_GRAB (hlist))
	{
	  remove_grab (hlist);
	  hlist->drag_button = 0;
	}

      GTK_HLIST_CLASS_FW (hlist)->resync_selection (hlist, NULL);

      hlist->button_actions[button] = button_actions;
    }
}
