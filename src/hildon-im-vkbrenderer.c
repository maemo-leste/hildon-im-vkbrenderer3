/**
  @file hildon-im-vkbrenderer3.c

  This file is part of libhildon-im-vkbrenderer3.

  This file may contain parts derived by disassembling of binaries under
  Nokia's copyright, see http://tablets-dev.nokia.com/maemo-dev-env-downloads.php

  The original licensing conditions apply to all those derived parts as well
  and you accept those by using this file.
*/

#include <string.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkbutton.h>
#include <imlayouts.h>

#include "hildon-vkb-renderer-marshal.h"
#include "hildon-im-vkbrenderer.h"

enum {
  INPUT = 0,
  ILLEGAL_INPUT,
  COMBINING_INPUT,
  TEMP_INPUT,
  LAST_SIGNAL
};

enum
{
  HILDON_VKB_RENDERER_PROP_DIMENSION = 1,
  HILDON_VKB_RENDERER_PROP_COLLECTION_FILE,
  HILDON_VKB_RENDERER_PROP_LAYOUT,
  HILDON_VKB_RENDERER_PROP_SUBLAYOUT,
  HILDON_VKB_RENDERER_PROP_LAYOUTS,
  HILDON_VKB_RENDERER_PROP_SUBLAYOUTS,
  HILDON_VKB_RENDERER_PROP_MODE,
  HILDON_VKB_RENDERER_PROP_OPTION,
  HILDON_VKB_RENDERER_PROP_STYLE_NORMAL,
  HILDON_VKB_RENDERER_PROP_STYLE_SPECIAL,
  HILDON_VKB_RENDERER_PROP_STYLE_SLIDE,
  HILDON_VKB_RENDERER_PROP_STYLE_WHITESPACE,
  HILDON_VKB_RENDERER_PROP_STYLE_TAB,
  HILDON_VKB_RENDERER_PROP_STYLE_BACKSPACE,
  HILDON_VKB_RENDERER_PROP_STYLE_SHIFT,
  HILDON_VKB_RENDERER_PROP_REPEAT_INTERVAL,
  HILDON_VKB_RENDERER_PROP_CYCLE_TIMEOUT,
  HILDON_VKB_RENDERER_PROP_WC_LANGUAGE,
  HILDON_VKB_RENDERER_PROP_GESTURE_RANGE
};

static guint
signals[LAST_SIGNAL] = { 0, };


#if 0
#define tracef g_warning("%s\n",__func__);
#else
#define tracef
#endif

typedef struct {
 gchar *style;
 GtkStyle* gtk_style;
}HildonVKBRendererKeyStyle;

struct _HildonVKBRendererPrivate
{
  vkb_layout_collection *layout_collection;
  vkb_layout *layout;
  vkb_sub_layout *sub_layout;
  vkb_layout *WC_language;
  vkb_sub_layout *current_sub_layout;
  int mode_bitmask;
  guint layout_type;
  guint num_sub_layouts;
  gboolean field_20;
  gboolean secondary_layout;
  gboolean field_28;
  PangoLayout* pango_layout;
  GdkPixmap *pixmap;
  GtkRequisition requisition;
  gboolean field_3C;
  vkb_key* pressed_key;
  vkb_key* dead_key;
  gdouble x;
  gdouble y;
  gboolean field_58;
  gboolean field_5C;
  gboolean field_60;
  gboolean field_64;
  int gesture_range;
  vkb_key* sliding_key;
  guint sliding_key_timer;
  guint sliding_key_timeout;
  HildonVKBRendererKeyStyle normal;
  HildonVKBRendererKeyStyle special;
  HildonVKBRendererKeyStyle slide;
  HildonVKBRendererKeyStyle whitespace;
  HildonVKBRendererKeyStyle tab;
  HildonVKBRendererKeyStyle backspace;
  HildonVKBRendererKeyStyle shift;
  guint key_repeat_init_timer;
  guint key_repeat_timer;
  guint key_repeat_interval;
  PangoFontDescription *font_desc;
  gboolean field_C0;
  gboolean shift_active;
};

struct _HildonVKBRendererLayoutInfo
{
  guint num_layouts;
  guint *type;
  gchar **label;
  guint num_rows;
};

#define HILDON_VKB_RENDERER_GET_PRIVATE(renderer) \
  ((HildonVKBRendererPrivate *)hildon_vkb_renderer_get_instance_private(renderer))

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
  HildonVKBRenderer, hildon_vkb_renderer, GTK_TYPE_WIDGET, 0,
  G_ADD_PRIVATE_DYNAMIC(HildonVKBRenderer)
);

static void hildon_vkb_renderer_key_update(HildonVKBRenderer *self, vkb_key *key, int shift_active, int repaint);
static void hildon_vkb_renderer_input_key(HildonVKBRenderer *self);
static void hildon_vkb_renderer_paint_pixmap(HildonVKBRenderer *self);
static void hildon_vkb_renderer_input_slide(HildonVKBRenderer *self, gboolean unk);

static gboolean hildon_vkb_renderer_sliding_key_timeout(void *data);
static gboolean hildon_vkb_renderer_key_repeat_init(void *data);

/**
 * Module functions
 *
 **/
void
dyn_vkb_renderer_exit()
{
}

GtkWidget *
dyn_vkb_renderer_create(const gchar *first_prop_name, va_list va_args)
{
  return (GtkWidget *)g_object_new_valist(HILDON_VKB_RENDERER_TYPE,
                                          first_prop_name, va_args);
}

void
dyn_vkb_renderer_init(GTypeModule *module)
{
  hildon_vkb_renderer_register_type(module);
}

/* Implementation of plugin interface starts here */

static void
hildon_vkb_renderer_clear(HildonVKBRenderer *self)
{
  HildonVKBRendererPrivate *priv;

  g_return_if_fail (HILDON_IS_VKB_RENDERER (self));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);

  if ( priv->sliding_key )
    hildon_vkb_renderer_input_slide(self, 1);
  if ( priv->sliding_key_timer )
  {
    g_source_remove(priv->sliding_key_timer);
    priv->sliding_key_timer = 0;
  }
}

static void
hildon_vkb_renderer_update_mode(HildonVKBRenderer *self, gboolean update_key)
{
  HildonVKBRendererPrivate *priv;
  vkb_sub_layout *sub_layout;
  vkb_key *key;
  gboolean needs_update=FALSE;

  tracef;

  g_return_if_fail (HILDON_IS_VKB_RENDERER (self));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);

  sub_layout = priv->sub_layout;
  if ( sub_layout && sub_layout->num_key_sections )
  {
    vkb_key_section *key_section;
    int section = 0;
    int i = 0;
    int j;

    while ( 1 )
    {
      key_section = &sub_layout->key_sections[section];
      if ( key_section->num_keys )
        break;

      section++;
      if ( sub_layout->num_key_sections <= section )
        return;
    }

    while ( 1 )
    {
      unsigned char gtk_state;

      key = &key_section->keys[i];

      if ((key->key_type & KEY_TYPE_HEXA) && key->sub_keys && key->num_sub_keys > 1)
      {
        if ( priv->secondary_layout )
          gtk_state = key->sub_keys[1].gtk_state;
        else
          gtk_state = key->sub_keys->gtk_state;

        if ( key->num_sub_keys )
        {
          j = 0;
          while ( 1 )
          {
            vkb_key *sub_key = &key->sub_keys[j];

            if (sub_key->key_flags & priv->mode_bitmask &&
                sub_key->labels && *sub_key->labels)
            {
              key->gtk_state = 0;
              ++j;
              if ( key->num_sub_keys <= j )
                break;
            }
            else
            {
              key->gtk_state = 4;
              ++j;
              if ( key->num_sub_keys <= j )
                break;
            }
          }
        }
        if ( priv->secondary_layout )
        {
          if ( key->sub_keys[1].gtk_state == gtk_state )
            goto update;
        }
        else
        {
          if ( key->sub_keys->gtk_state == gtk_state )
            goto update;
        }
      }
      else
      {
        unsigned char old_gtk_state = key->gtk_state;

        if ((key->key_flags & priv->mode_bitmask &&
             (key->labels && (key->key_type == KEY_TYPE_SLIDING || *key->labels))) ||
            (key->key_flags & priv->mode_bitmask & (KEY_TYPE_TAB|KEY_TYPE_WHITESPACE)) ||
            (key->key_flags & KEY_TYPE_SHIFT))
          key->gtk_state = 0;
        else
          key->gtk_state = 4;
        if ( key->gtk_state == old_gtk_state )
          goto update;
      }
      needs_update = TRUE;
update:
      if(update_key && needs_update)
      {
        hildon_vkb_renderer_key_update(self, key, -1, 1);
        key_section = &sub_layout->key_sections[section];
      }
      ++i;
      if ( key_section->num_keys <= i )
      {
        section++;
        if ( sub_layout->num_key_sections <= section )
          return;
      }
    }
  }
}

static void
hildon_vkb_renderer_update_pixmap(HildonVKBRenderer *self)
{
  HildonVKBRendererPrivate *priv;

  tracef;
  g_return_if_fail (HILDON_IS_VKB_RENDERER (self));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);

  imlayout_vkb_init_buttons(priv->layout_collection,
                            priv->layout,
                            GTK_WIDGET(self)->allocation.width,
                            GTK_WIDGET(self)->allocation.height);
  hildon_vkb_renderer_update_mode(self, 0);
  hildon_vkb_renderer_paint_pixmap(self);
}

static gboolean
hildon_vkb_renderer_load_layout(HildonVKBRenderer *self, int layout)
{
  HildonVKBRendererPrivate *priv;
  tracef;

  g_return_val_if_fail (HILDON_IS_VKB_RENDERER (self),FALSE);

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);

  if ( priv->layout_collection )
  {
    if ( !priv->layout )
    {
      priv->layout = imlayout_vkb_get_layout(priv->layout_collection, priv->layout_type);

      if ( !priv->layout )
      {
        g_warning("Set layout type does not exist in current collection, will load default layout in current collection\n");

        priv->layout = imlayout_vkb_get_layout(
                               priv->layout_collection,
                               *priv->layout_collection->layout_types);

        if ( !priv->layout )
          goto error;
      }
    }
    if ( !layout || priv->sub_layout )
      return TRUE;

    hildon_vkb_renderer_clear(self);

    if ( priv->num_sub_layouts >= priv->layout->num_sub_layouts )
    {
      g_warning("Set sublayout type does not exist in current layout, will load default sublayout in current layout\n");
      priv->sub_layout = priv->layout->sub_layouts;
      priv->num_sub_layouts = 0;
    }
    else
      priv->sub_layout = &priv->layout->sub_layouts[priv->num_sub_layouts];

    if ( priv->sub_layout )
    {
      hildon_vkb_renderer_update_pixmap(self);
      if ( priv->WC_language )
      {
        imlayout_vkb_free_layout(priv->WC_language);
        priv->WC_language = 0;
      }
      priv->current_sub_layout = 0;
      return TRUE;
    }
    else
    {
error:
      g_warning("Loading default layout fails\n");
    }
  }

  return FALSE;
}

guint
hildon_vkb_renderer_get_numeric_sub(HildonVKBRenderer *renderer)
{
  HildonVKBRendererPrivate *priv;
  tracef;

  g_return_val_if_fail (HILDON_IS_VKB_RENDERER (renderer),-1);

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(renderer);

  if(!priv->layout)
  {
    hildon_vkb_renderer_load_layout(renderer, LAYOUT_TYPE_NORMAL);

    if(!priv->layout)
      return -1;
  }

  return priv->layout->numeric;
}

void
hildon_vkb_renderer_set_variance_layout(HildonVKBRenderer *renderer)
{
  HildonVKBRendererPrivate *priv;
  tracef;

  g_return_if_fail (HILDON_IS_VKB_RENDERER (renderer));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(renderer);

  if(!priv->sub_layout)
    hildon_vkb_renderer_load_layout(renderer, 1);

  if (priv->sub_layout && !(priv->sub_layout->variance_index & 0x80))
  {
    g_object_set(renderer, "sub", priv->sub_layout->variance_index, NULL);
    gtk_widget_queue_draw(GTK_WIDGET(renderer));
  }
}

static void
update_style(HildonVKBRenderer *self, const gchar *new_style, guint prop_id, gboolean keep)
{
  HildonVKBRendererPrivate *priv;
  HildonVKBRendererKeyStyle *key_style;
tracef;

  g_return_if_fail (HILDON_IS_VKB_RENDERER (self));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);

  switch(prop_id)
  {
    case HILDON_VKB_RENDERER_PROP_STYLE_NORMAL:
      key_style=&priv->normal;
      break;
    case HILDON_VKB_RENDERER_PROP_STYLE_SPECIAL:
      key_style=&priv->special;
      break;
    case HILDON_VKB_RENDERER_PROP_STYLE_SLIDE:
      key_style=&priv->slide;
      break;
    case HILDON_VKB_RENDERER_PROP_STYLE_WHITESPACE:
      key_style=&priv->whitespace;
      break;
    case HILDON_VKB_RENDERER_PROP_STYLE_TAB:
      key_style=&priv->tab;
      break;
    case HILDON_VKB_RENDERER_PROP_STYLE_BACKSPACE:
      key_style=&priv->backspace;
      break;
    case HILDON_VKB_RENDERER_PROP_STYLE_SHIFT:
      key_style=&priv->shift;
      break;
    default:
      return;
  }

  if(!keep)
  {
    if(key_style->style)
      g_free(key_style->style);
    key_style->style = g_strdup(new_style);
  }

  key_style->gtk_style = gtk_rc_get_style_by_paths(gtk_widget_get_settings(GTK_WIDGET(self)),key_style->style,NULL,GTK_TYPE_BUTTON);

  if(!key_style->gtk_style)
    key_style->gtk_style = gtk_widget_get_default_style();

  hildon_vkb_renderer_paint_pixmap(self);
}

static void
theme_changed(GtkWidget *widget, GtkStyle *previous_style, void *user_data)
{
  HildonVKBRendererPrivate *priv;
  tracef;

  g_return_if_fail (HILDON_IS_VKB_RENDERER (user_data));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE (HILDON_VKB_RENDERER(user_data));


  if ( priv->font_desc )
    pango_font_description_free(priv->font_desc);

  priv->font_desc = pango_font_description_copy(widget->style->font_desc);

  pango_font_description_set_size(priv->font_desc, 1.2 * pango_font_description_get_size(priv->font_desc));

  if ( priv->pango_layout )
    pango_layout_set_font_description(priv->pango_layout, priv->font_desc);

  update_style(HILDON_VKB_RENDERER(widget), priv->normal.style, HILDON_VKB_RENDERER_PROP_STYLE_NORMAL, 1);
  update_style(HILDON_VKB_RENDERER(widget), priv->special.style, HILDON_VKB_RENDERER_PROP_STYLE_SPECIAL, 1);
  update_style(HILDON_VKB_RENDERER(widget), priv->slide.style, HILDON_VKB_RENDERER_PROP_STYLE_SLIDE, 1);
  update_style(HILDON_VKB_RENDERER(widget), priv->whitespace.style, HILDON_VKB_RENDERER_PROP_STYLE_WHITESPACE, 1);
  update_style(HILDON_VKB_RENDERER(widget), priv->tab.style, HILDON_VKB_RENDERER_PROP_STYLE_TAB, 1);
  update_style(HILDON_VKB_RENDERER(widget), priv->backspace.style, HILDON_VKB_RENDERER_PROP_STYLE_BACKSPACE, 1);
  update_style(HILDON_VKB_RENDERER(widget), priv->shift.style, HILDON_VKB_RENDERER_PROP_STYLE_SHIFT, 1);
}

static void
hildon_vkb_renderer_init (HildonVKBRenderer *self)
{
  HildonVKBRendererPrivate *priv;
  tracef;

  g_return_if_fail (HILDON_IS_VKB_RENDERER (self));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);
  priv->layout_collection = NULL;
  priv->layout = NULL;
  priv->sub_layout = NULL;
  priv->WC_language = NULL;
  priv->current_sub_layout = 0;
  priv->mode_bitmask = 0;
  priv->layout_type = 0;
  priv->field_20 = FALSE;
  priv->field_28 = FALSE;
  priv->pixmap = 0;
  priv->field_3C = FALSE;
  priv->pressed_key = NULL;
  priv->x = 0;
  priv->y = 0;
  priv->field_58 = TRUE;
  priv->field_5C = FALSE;
  priv->field_60 = FALSE;
  priv->field_64 = TRUE;
  priv->sliding_key_timer = 0;
  priv->normal.style  = NULL;
  priv->special.style = NULL;
  priv->slide.style = NULL;
  priv->whitespace.style  = NULL;
  priv->tab.style = NULL;
  priv->backspace.style = NULL;
  priv->shift.style = NULL;
  priv->key_repeat_init_timer = 0;
  priv->key_repeat_timer = 0;
  priv->field_C0 = FALSE;
  priv->shift_active = FALSE;

  g_signal_connect_data(self, "style-set", G_CALLBACK(theme_changed), self, 0, 0);
}

gboolean
hildon_vkb_renderer_get_shift_active(HildonVKBRenderer * renderer)
{
  tracef;
  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(renderer),FALSE);


  return HILDON_VKB_RENDERER_GET_PRIVATE(renderer)->shift_active;
}

void
hildon_vkb_renderer_set_shift_active(HildonVKBRenderer *renderer, gboolean value)
{
  tracef;
  g_return_if_fail(HILDON_IS_VKB_RENDERER(renderer));
  HILDON_VKB_RENDERER_GET_PRIVATE(renderer)->shift_active = value;
}

guint
hildon_vkb_renderer_get_pressed_key_mode(HildonVKBRenderer * renderer)
{
  tracef;
  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(renderer),0);
  return HILDON_VKB_RENDERER_GET_PRIVATE(renderer)->pressed_key->key_flags;
}

static void
hildon_vkb_renderer_release_cleanup(HildonVKBRendererPrivate *priv)
{
  g_return_if_fail(priv != NULL);

  priv->pressed_key = NULL;
  priv->x = 0;
  priv->y = 0;
  priv->field_58 = TRUE;
  priv->field_5C = FALSE;
  priv->field_60 = FALSE;
  priv->field_64 = TRUE;
  priv->field_20 = FALSE;
}


void
layout_info_free(HildonVKBRendererLayoutInfo *layout_info)
{
  if (layout_info)
  {
    if (layout_info->label)
    {
      int i;

      for (i = 0; i < layout_info->num_layouts; i++)
        g_free(layout_info->label[i]);

      g_free(layout_info->label);
    }

    g_free(layout_info->type);
    g_free(layout_info);
  }
}

static void
hildon_vkb_renderer_get_property (GObject *object,
              guint prop_id,
              GValue *value,
              GParamSpec *pspec)
{
  HildonVKBRendererPrivate *priv;

  g_return_if_fail (HILDON_IS_VKB_RENDERER (object));
  priv = HILDON_VKB_RENDERER_GET_PRIVATE (HILDON_VKB_RENDERER(object));

  switch (prop_id)
  {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
    case HILDON_VKB_RENDERER_PROP_GESTURE_RANGE:
      g_value_set_int(value, priv->gesture_range);
      break;
    case HILDON_VKB_RENDERER_PROP_WC_LANGUAGE:
      if ( !priv->layout_collection )
        g_value_set_string(value, 0);
      else
        g_value_set_string(value, priv->layout_collection->wc);
      break;
    case HILDON_VKB_RENDERER_PROP_CYCLE_TIMEOUT:
      g_value_set_int(value, priv->sliding_key_timeout);
      break;
    case HILDON_VKB_RENDERER_PROP_REPEAT_INTERVAL:
      g_value_set_int(value, priv->key_repeat_interval);
      break;
    case HILDON_VKB_RENDERER_PROP_STYLE_SHIFT:
      g_value_set_string(value, priv->shift.style);
      break;
    case HILDON_VKB_RENDERER_PROP_STYLE_BACKSPACE:
      g_value_set_string(value, priv->backspace.style);
      break;
    case HILDON_VKB_RENDERER_PROP_STYLE_TAB:
      g_value_set_string(value, priv->tab.style);
      break;
    case HILDON_VKB_RENDERER_PROP_STYLE_WHITESPACE:
      g_value_set_string(value, priv->whitespace.style);
      return;
    case HILDON_VKB_RENDERER_PROP_STYLE_SLIDE:
      g_value_set_string(value, priv->slide.style);
      break;
    case HILDON_VKB_RENDERER_PROP_STYLE_SPECIAL:
      g_value_set_string(value, priv->special.style);
      break;
    case HILDON_VKB_RENDERER_PROP_STYLE_NORMAL:
      g_value_set_string(value, priv->normal.style);
      break;
    case HILDON_VKB_RENDERER_PROP_OPTION:
      g_value_set_boolean(value, priv->secondary_layout);
      return;
    case HILDON_VKB_RENDERER_PROP_MODE:
      g_value_set_int(value, priv->mode_bitmask);
      break;
    case HILDON_VKB_RENDERER_PROP_SUBLAYOUTS:
      if ( !priv->layout && !hildon_vkb_renderer_load_layout(HILDON_VKB_RENDERER(object), 1) )
      {
        g_value_set_pointer(value, 0);
        break;
      }
      else
      {
        HildonVKBRendererLayoutInfo *layout_info = (HildonVKBRendererLayoutInfo *)g_malloc0(sizeof(HildonVKBRendererLayoutInfo));

        layout_info->num_layouts = priv->layout->num_sub_layouts;
        layout_info->type = (unsigned int *)g_malloc0(sizeof(guint*) * layout_info->num_layouts);
        layout_info->label = (gchar **)g_malloc0(sizeof(gchar*) * layout_info->num_layouts);
        if ( layout_info->num_layouts > 0 )
        {
          int i = 0;
          do
          {
            layout_info->type[i] = priv->layout->sub_layouts[i].type;
            layout_info->label[i] = g_strdup(priv->layout->sub_layouts[i].label);
            ++i;
          }
          while ( layout_info->num_layouts > i );
        }
        if ( priv->sub_layout || (priv->sub_layout = priv->current_sub_layout) != 0 )
          layout_info->num_rows = priv->sub_layout->key_sections->num_rows;
        else
          layout_info->num_rows = -1;
        g_value_set_pointer(value, layout_info);
      }
      break;
    case HILDON_VKB_RENDERER_PROP_LAYOUTS:
      if ( priv->layout_collection )
      {
        HildonVKBRendererLayoutInfo *layout_info = (HildonVKBRendererLayoutInfo *)g_malloc0(sizeof(HildonVKBRendererLayoutInfo));

        layout_info->num_layouts = priv->layout_collection->num_layouts;
        layout_info->type = (guint *)g_malloc0(sizeof(guint) * layout_info->num_layouts);
        layout_info->label = 0;

        if ( layout_info->num_layouts > 0 )
        {
          int i = 0;
          do
          {
            layout_info->type [i] = priv->layout_collection->layout_types[i];
            i++;
          }
          while ( i != layout_info->num_layouts );
        }

        if ( priv->sub_layout || (priv->sub_layout = priv->current_sub_layout) != 0 )
          layout_info->num_rows = priv->sub_layout->key_sections->num_rows;
        else
          layout_info->num_rows = -1;

        g_value_set_pointer(value, layout_info);
      }
      else
        g_value_set_pointer(value, 0);
      break;
    case HILDON_VKB_RENDERER_PROP_SUBLAYOUT:
      if ( !priv->sub_layout && !hildon_vkb_renderer_load_layout(HILDON_VKB_RENDERER(object), 1) )
        g_value_set_int(value, 0);
      else
        g_value_set_int(value, priv->num_sub_layouts);
      break;
    case HILDON_VKB_RENDERER_PROP_COLLECTION_FILE:
      if ( priv->layout_collection )
        g_value_set_string(value, priv->layout_collection->filename);
      else
        g_value_set_string(value, 0);
      break;
    case HILDON_VKB_RENDERER_PROP_LAYOUT:
      if ( priv->layout || hildon_vkb_renderer_load_layout(HILDON_VKB_RENDERER(object), 1) )
        g_value_set_int(value, priv->layout_type);
      else
        g_value_set_int(value, 0);
      break;
    case HILDON_VKB_RENDERER_PROP_DIMENSION:
      g_value_set_boxed(value, &priv->requisition);
      break;
  }
}
void hildon_vkb_renderer_update_option(GObject * self)
{
  HildonVKBRenderer *renderer;
  vkb_key_section * key_section;
  vkb_key *key;
  vkb_sub_layout * sub_layout;
  HildonVKBRendererPrivate *priv;

  tracef;

  g_return_if_fail (HILDON_IS_VKB_RENDERER (self));

  renderer = HILDON_VKB_RENDERER(self);
  priv = HILDON_VKB_RENDERER_GET_PRIVATE(renderer);

  sub_layout = priv->sub_layout;

  if ( sub_layout && sub_layout->num_key_sections )
  {
    int section = 0;
    while ( 1 )
    {
      key_section = &sub_layout->key_sections[section];
      if ( key_section->num_keys )
      {
        int i = 0;
        do
        {
          while ( 1 )
          {
            key = &key_section->keys[i];
            if ( key->key_type & 4 )
              break;
            i++;
            key_section = &sub_layout->key_sections[section];
            if ( key_section->num_keys <= i )
              goto LABEL_27;
          }
          hildon_vkb_renderer_key_update(renderer, key, -1, 1);
          i++;
          key_section = &sub_layout->key_sections[section];
        }
        while ( key_section->num_keys > i );
      }
LABEL_27:
      section++;
      if ( sub_layout->num_key_sections <= section )
        return;
    }
  }
}

static void
hildon_vkb_renderer_set_property(GObject *object,
             guint prop_id,
             const GValue *value,
             GParamSpec *pspec)
{
  HildonVKBRendererPrivate *priv;

  tracef;

  g_return_if_fail(HILDON_IS_VKB_RENDERER (object));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE (HILDON_VKB_RENDERER(object));

  switch (prop_id)
  {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
    case HILDON_VKB_RENDERER_PROP_GESTURE_RANGE:
      priv->gesture_range = g_value_get_int(value);
      break;
    case HILDON_VKB_RENDERER_PROP_CYCLE_TIMEOUT:
      priv->sliding_key_timeout = g_value_get_int(value);
      break;
    case HILDON_VKB_RENDERER_PROP_REPEAT_INTERVAL:
      priv->key_repeat_interval = g_value_get_int(value);
      break;
    case HILDON_VKB_RENDERER_PROP_STYLE_NORMAL:
    case HILDON_VKB_RENDERER_PROP_STYLE_SPECIAL:
    case HILDON_VKB_RENDERER_PROP_STYLE_SLIDE:
    case HILDON_VKB_RENDERER_PROP_STYLE_WHITESPACE:
    case HILDON_VKB_RENDERER_PROP_STYLE_TAB:
    case HILDON_VKB_RENDERER_PROP_STYLE_BACKSPACE:
    case HILDON_VKB_RENDERER_PROP_STYLE_SHIFT:
      update_style(HILDON_VKB_RENDERER (object), g_value_get_string(value), prop_id, 0);
      break;
    case HILDON_VKB_RENDERER_PROP_OPTION:
      if ( priv->sub_layout )
      {
        if ( priv->secondary_layout == g_value_get_boolean(value) )
          return;
        priv->secondary_layout = g_value_get_boolean(value);
        hildon_vkb_renderer_update_option(object);
      }
      else
        priv->secondary_layout = g_value_get_boolean(value);
      break;
    case HILDON_VKB_RENDERER_PROP_MODE:
      if ( priv->sub_layout )
      {
        if ( priv->mode_bitmask != g_value_get_int(value) )
        {
          priv->mode_bitmask = g_value_get_int(value);
          hildon_vkb_renderer_update_mode(HILDON_VKB_RENDERER (object), 1);
        }
      }
      else
        priv->mode_bitmask = g_value_get_int(value);
      break;
    case HILDON_VKB_RENDERER_PROP_SUBLAYOUT:
      priv->num_sub_layouts = g_value_get_int(value);
      if ( priv->sub_layout )
      {
        priv->current_sub_layout = priv->sub_layout;
        priv->field_20 = TRUE;
      }
      priv->sub_layout = 0;
    break;
    case HILDON_VKB_RENDERER_PROP_COLLECTION_FILE:
    {
      const gchar* collection_file = g_value_get_string(value);
      if ( !collection_file )
        return;
      if ( !priv->layout_collection )
        goto LABEL_39;
      if ( strcmp(priv->layout_collection->filename, collection_file) )
      {
        imlayout_vkb_free_layout_collection(priv->layout_collection);
  LABEL_39:
        priv->layout_collection = imlayout_vkb_load_file(collection_file);
        if ( !priv->layout )
          goto LABEL_43;
        goto LABEL_40;
      }
      break;
    }
    case HILDON_VKB_RENDERER_PROP_DIMENSION:
    {
      GtkRequisition * requisition = (GtkRequisition *)g_value_get_boxed(value);

      priv->requisition.height = requisition->height;
      priv->requisition.width = requisition->width;;
      break;
    }
    case HILDON_VKB_RENDERER_PROP_LAYOUT:
      priv->layout_type = g_value_get_int(value);
      if ( priv->layout )
      {
LABEL_40:
        if ( priv->WC_language )
          imlayout_vkb_free_layout(priv->WC_language);

        priv->WC_language = priv->layout;
        priv->current_sub_layout = priv->sub_layout;
      }
LABEL_43:
      priv->layout = 0;
      priv->sub_layout = 0;

      if ( priv->dead_key )
        g_signal_emit(HILDON_VKB_RENDERER (object), signals[COMBINING_INPUT], 0, 0, 0);

      priv->dead_key = 0;
      break;
  }
}


void
hildon_vkb_renderer_marshal_VOID__STRING_BOOLEAN (GClosure     *closure,
                                                  GValue       *return_value G_GNUC_UNUSED,
                                                  guint         n_param_values,
                                                  const GValue *param_values,
                                                  gpointer      invocation_hint G_GNUC_UNUSED,
                                                  gpointer      marshal_data)
{
  typedef void (*GMarshalFunc_VOID__STRING_BOOLEAN) (gpointer     data1,
                                                     gpointer     arg_1,
                                                     gboolean     arg_2,
                                                     gpointer     data2);
  register GMarshalFunc_VOID__STRING_BOOLEAN callback;
  register GCClosure *cc = (GCClosure*) closure;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }
  callback = (GMarshalFunc_VOID__STRING_BOOLEAN) (marshal_data ? marshal_data : cc->callback);

  callback (data1,
            g_marshal_value_peek_string (param_values + 1),
            g_marshal_value_peek_boolean (param_values + 2),
            data2);
}

static void
hildon_vkb_renderer_paint_pixmap(HildonVKBRenderer *self)
{
  vkb_sub_layout *sub_layout;
  vkb_key_section *key_section;
  unsigned int i;

  HildonVKBRendererPrivate *priv;
  tracef;

  g_return_if_fail(HILDON_IS_VKB_RENDERER(self));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);

  if ( priv->sub_layout )
  {
    if ( priv->pixmap )
    {
      priv->field_28 = FALSE;
      imlayout_vkb_init_buttons(priv->layout_collection,
                                priv->layout,
                                GTK_WIDGET(self)->allocation.width,
                                GTK_WIDGET(self)->allocation.height);

      gdk_draw_rectangle(priv->pixmap,
                         GTK_WIDGET(self)->style->bg_gc[0],
                         1, 0, 0,
                         priv->requisition.width,
                         priv->requisition.height);
      sub_layout = priv->sub_layout;
      if ( sub_layout->num_key_sections )
      {
        int section = 0;
        do
        {
          key_section = &sub_layout->key_sections[section];
          if ( key_section->num_keys )
          {
            i = 0;
            do
            {
              vkb_key *key = &key_section->keys[i];

              if(!priv->dead_key ||
                 g_strcmp0((gchar *)priv->dead_key->labels,
                           (gchar *)key->labels))
              {
                if((key->key_flags & KEY_TYPE_SHIFT) == KEY_TYPE_SHIFT)
                  key->gtk_state = priv->shift_active != 0;
              }
              else
              {
                key->gtk_state = 1;
                priv->dead_key->gtk_state = 1;
                priv->field_20 = FALSE;
              }
              hildon_vkb_renderer_key_update(self, key, -1, 0);
              i++;
              sub_layout = priv->sub_layout;
              key_section = &sub_layout->key_sections[section];
            }
            while ( key_section->num_keys > i );
          }
          ++section;
        }
        while ( sub_layout->num_key_sections > section );
      }
      priv->field_3C = TRUE;
    }
  }
  else
  {
    priv->field_28 = TRUE;
  }
}

void hildon_vkb_renderer_size_allocate(GtkWidget *widget, GdkRectangle *allocation)
{
  GdkRectangle r;
tracef;
  memcpy(&r, &widget->allocation, sizeof(r));

  g_return_if_fail(HILDON_IS_VKB_RENDERER(widget));

  memcpy(&widget->allocation, allocation, sizeof(GtkAllocation));

  if(GTK_WIDGET_REALIZED(GTK_OBJECT(widget)))
    gdk_window_move_resize(widget->window, allocation->x, allocation->y, allocation->width, allocation->height);

  if ( allocation->width != r.width || allocation->height != r.height )
    hildon_vkb_renderer_paint_pixmap(HILDON_VKB_RENDERER(widget));
}

void
hildon_vkb_renderer_realize(GtkWidget *widget)
{
  GdkWindow *parent;

  GdkScreen * gdkscreen;
  GdkDisplay *gdkdisplay;

  int root_depth;

  HildonVKBRendererPrivate *priv;
  cairo_t *cr;
  GdkWindowAttr attributes;
  tracef;

  g_return_if_fail(HILDON_IS_VKB_RENDERER(widget));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(HILDON_VKB_RENDERER(widget));

  GTK_OBJECT(widget)->flags |= GDK_WINDOW_STATE_BELOW;

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = gtk_widget_get_events(widget) | 0x312;
  attributes.visual = (GdkVisual *)gtk_widget_get_visual(widget);
  attributes.colormap = (GdkColormap *)gtk_widget_get_colormap(widget);
  attributes.wclass = 0;
  parent = (GdkWindow *)gtk_widget_get_parent_window(widget);

  widget->window = gdk_window_new(parent, &attributes, 0x6Cu);
  gdk_window_set_user_data(widget->window, widget);

  widget->style = gtk_style_attach(widget->style, widget->window);
  gtk_style_set_background(widget->style, widget->window, 0);

  gdkscreen = gtk_widget_get_screen(widget);
  gdkdisplay = gdk_screen_get_display(gdkscreen);

  root_depth = DefaultDepthOfScreen(
        XScreenOfDisplay(gdk_x11_display_get_xdisplay(gdkdisplay),
                         gdk_x11_screen_get_screen_number(gdkscreen)));

  priv->pixmap = gdk_pixmap_new(0,
                                priv->requisition.width,
                                priv->requisition.height,
                                root_depth);

  cr = gdk_cairo_create(widget->window);
  priv->pango_layout = pango_cairo_create_layout(cr);
  if ( priv->font_desc )
    pango_font_description_free(priv->font_desc);
  priv->font_desc = pango_font_description_copy(widget->style->font_desc);

  pango_font_description_set_size(priv->font_desc,
                                  1.2 * pango_font_description_get_size(priv->font_desc));

  if ( priv->pango_layout )
    pango_layout_set_font_description(priv->pango_layout,
                                      priv->font_desc);

  cairo_destroy(cr);
}

void hildon_vkb_renderer_unrealize(GtkWidget *widget)
{
  HildonVKBRendererPrivate *priv;
  tracef;

  g_return_if_fail(HILDON_IS_VKB_RENDERER(widget));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(HILDON_VKB_RENDERER(widget));

  gdk_x11_display_get_xdisplay(gdk_screen_get_display(gtk_widget_get_screen(widget)));

  gdk_x11_visual_get_xvisual(gdk_drawable_get_visual(widget->window));

  gdk_x11_colormap_get_xcolormap(gdk_drawable_get_colormap(widget->window));

  if ( priv->pixmap )
  {
    g_object_unref(priv->pixmap);
    priv->pixmap = NULL;
  }
  if ( priv->pango_layout )
  {
    g_object_unref(priv->pango_layout);
    priv->pango_layout = NULL;
  }

  if (GTK_WIDGET_CLASS(hildon_vkb_renderer_parent_class)->unrealize)
  {
    GTK_WIDGET_CLASS(hildon_vkb_renderer_parent_class)->unrealize(widget);
  }

}

static void
hildon_vkb_renderer_finalize(GObject *obj)
{
  HildonVKBRendererPrivate *priv;
  tracef;

  g_return_if_fail(HILDON_IS_VKB_RENDERER(obj));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(HILDON_VKB_RENDERER(obj));
  gdk_x11_display_get_xdisplay(gdk_screen_get_display(gtk_widget_get_screen(GTK_WIDGET(obj))));

  if ( priv->layout )
    imlayout_vkb_free_layout(priv->layout);
  if ( priv->WC_language )
    imlayout_vkb_free_layout(priv->WC_language);
  if ( priv->layout_collection )
    imlayout_vkb_free_layout_collection(priv->layout_collection);

  g_free(priv->normal.style);
  g_free(priv->special.style);
  g_free(priv->slide.style);
  g_free(priv->whitespace.style);
  g_free(priv->tab.style);
  g_free(priv->backspace.style);
  g_free(priv->shift.style);

  if ( priv->key_repeat_timer )
    g_source_remove(priv->key_repeat_timer);

  if ( priv->key_repeat_init_timer )
    g_source_remove(priv->key_repeat_init_timer);

  if ( priv->sliding_key_timer )
    g_source_remove(priv->sliding_key_timer);

  if (G_OBJECT_CLASS(hildon_vkb_renderer_parent_class)->finalize)
  {
    G_OBJECT_CLASS(hildon_vkb_renderer_parent_class)->finalize(obj);
  }
}

static int
hildon_vkb_renderer_font_has_char(HildonVKBRenderer *self,gunichar uc)
{

  cairo_t* ct;
  PangoLayout * layout;
  char c;
  gint num_chars;
  int unknown_glyphs_count;
  tracef;

  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(self),TRUE);

  ct = gdk_cairo_create(GTK_WIDGET(self)->window);
  num_chars = g_unichar_to_utf8(uc, &c);
  layout = pango_cairo_create_layout(ct);
  pango_layout_set_text(layout, &c, num_chars);
  unknown_glyphs_count = pango_layout_get_unknown_glyphs_count(layout);
  g_object_unref(layout);
  cairo_destroy(ct);

  return unknown_glyphs_count==0;
}

static void
hildon_vkb_renderer_accent_combine_input(HildonVKBRenderer *self, const char *combine, gboolean unk)
{
  HildonVKBRendererPrivate *priv;
  vkb_key *key;
  gunichar uc_label;
  gchar *s_normal;
  gchar* s;
  gunichar str[2];
  tracef;

  g_return_if_fail(HILDON_IS_VKB_RENDERER(self));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);

    key = priv->dead_key;
    if ( key )
    {
      uc_label = g_utf8_get_char((gchar *)key->labels);
      if ( uc_label == 0xB8 )
      {
        str[1] = 0x327u;
        goto LABEL_14;
      }
      if ( uc_label > 0xB8 )
      {
        if ( uc_label == 0x2D9 )
        {
          str[1] = 0x307u;
          goto LABEL_14;
        }
        if ( uc_label <= 0x2D9 )
        {
          if ( uc_label == 0x2C7 )
          {
            str[1] = 0x30Cu;
            goto LABEL_14;
          }
          if ( uc_label > 0x2C7 )
          {
            if ( uc_label == 0x2C8 )
            {
              str[1] = 0x30Du;
              goto LABEL_14;
            }
            if ( uc_label == 0x2D8 )
            {
              str[1] = 0x306u;
              goto LABEL_14;
            }
          }
          else
          {
            if ( uc_label == 0x2BD )
            {
              str[1] = 0x314u;
              goto LABEL_14;
            }
          }
          goto LABEL_13;
        }
        if ( uc_label == 0x2DB )
        {
          str[1] = 0x328u;
          goto LABEL_14;
        }
        if ( uc_label >= 0x2DB )
        {
          if ( uc_label != 0x2DD )
          {
            if ( uc_label == 0x385 )
            {
              str[1] = 0x344u;
              goto LABEL_14;
            }
            goto LABEL_13;
          }
          goto LABEL_20;
        }
      }
      else
      {
        if ( uc_label == 0x7E )
        {
          str[1] = 0x303u;
          goto LABEL_14;
        }
        if ( uc_label <= 0x7E )
        {
          if ( uc_label != 0x27 )
          {
            if ( uc_label > 0x27 )
            {
              if ( uc_label == 0x5E )
              {
                str[1] = 0x302u;
                goto LABEL_14;
              }
              if ( uc_label == 0x60 )
              {
                str[1] = 0x300u;
                goto LABEL_14;
              }
              goto LABEL_13;
            }
            if ( uc_label != 0x22 )
            {
LABEL_13:
              str[1] = 0;
LABEL_14:
              str[0] = g_utf8_get_char(combine);
              s = g_ucs4_to_utf8(str, 2, 0, 0, 0);
              s_normal = g_utf8_normalize(s, -1, G_NORMALIZE_DEFAULT_COMPOSE);

              if ( strlen(s) > strlen(s_normal) &&
                   strcmp(s, s_normal) &&
                   hildon_vkb_renderer_font_has_char(HILDON_VKB_RENDERER(self),g_utf8_get_char(s_normal)))
              {
                g_signal_emit(self, signals[INPUT], 0, s_normal, unk);
                g_free(s);
                g_free(s_normal);
                return;
              }
              g_signal_emit(self, signals[INPUT], 0, combine, unk);
              g_free(s);
              g_free(s_normal);
              return;
            }
LABEL_20:
            str[1] = 0x30Bu;
            goto LABEL_14;
          }
LABEL_44:
          str[1] = 0x301u;
          goto LABEL_14;
        }
        if ( uc_label == 0xAF )
        {
          str[1] = 0x304u;
          goto LABEL_14;
        }
        if ( uc_label <= 0xAF )
        {
          if ( uc_label == 168 )
          {
            str[1] = 0x308u;
            goto LABEL_14;
          }
          goto LABEL_13;
        }
        if ( uc_label != 0xB0 )
        {
          if ( uc_label != 0xB4 )
            goto LABEL_13;
          goto LABEL_44;
        }
      }
      str[1] = 0x30Au;
      goto LABEL_14;
    }
}

static void
hildon_vkb_renderer_input_slide(HildonVKBRenderer *self, gboolean unk)
{
  vkb_key *sliding_key;
  HildonVKBRendererPrivate *priv;
  gchar *label;
  tracef;

  g_return_if_fail(HILDON_IS_VKB_RENDERER(self));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);
  sliding_key = priv->sliding_key;

  if (sliding_key)
  {
    if (unk)
    {
      if (sliding_key->current_slide_key)
        label = sliding_key->labels[sliding_key->current_slide_key - 1];
      else
        label = sliding_key->labels[sliding_key->byte_count - 1];

      sliding_key->width = 0;
      priv->sliding_key->current_slide_key = 0;

      gtk_widget_queue_draw_area(GTK_WIDGET(self),
                                 priv->sliding_key->left,
                                 priv->sliding_key->top,
                                 priv->sliding_key->right - priv->sliding_key->left,
                                 priv->sliding_key->bottom - priv->sliding_key->top);

      priv->sliding_key = 0;
      if ( priv->sliding_key_timer )
      {
        g_source_remove(priv->sliding_key_timer);
        priv->sliding_key_timer = 0;
      }
      if ( priv->dead_key )
        goto has_dead_key;
    }
    else
    {
      label = sliding_key->labels[sliding_key->current_slide_key];

      if (priv->dead_key)
      {
has_dead_key:
        hildon_vkb_renderer_accent_combine_input(self, label, unk);
        return;
      }
    }
    g_signal_emit(self, signals[INPUT], 0, label, unk);
  }
}

static gboolean
is_shift_or_dead_key(vkb_key *key)
{
  if ( key->key_flags & KEY_TYPE_DEAD )
    return TRUE;

  return ((key->key_flags & KEY_TYPE_SHIFT) == KEY_TYPE_SHIFT);
}

static gboolean
hildon_vkb_renderer_button_release(GtkWidget *widget, GdkEventButton *event)
{
  vkb_key *pressed_key;
  int gesture_range;
  int timer;
  vkb_key *dead_key;

  unsigned short key_flags;
  int shift;

  tracef;

  HildonVKBRenderer *self;
  HildonVKBRendererPrivate *priv;
  gboolean b;

  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(widget),TRUE);

  self = HILDON_VKB_RENDERER(widget);

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);

  if ( event->button != 1 || !priv->pressed_key || !priv->sub_layout )
    goto out;
  priv->field_C0 = FALSE;
  pressed_key = priv->pressed_key;
  gesture_range = priv->gesture_range;
  if(event->x <= ((int)pressed_key->left - gesture_range) ||
     event->x > (pressed_key->right + gesture_range) ||
     event->y <= ((int)pressed_key->top - gesture_range) ||
     event->y > (pressed_key->bottom + gesture_range))
  {
    b = 0;
  }
  else
  {
    timer = priv->key_repeat_timer;
    if ( timer )
    {
      b = 0;
LABEL_14:
      g_source_remove(timer);
      priv->key_repeat_timer = 0;
      pressed_key = priv->pressed_key;
      goto LABEL_15;
    }

    if ( is_shift_or_dead_key(pressed_key) )
    {
      b = 1;
      goto LABEL_15;
    }

    if(priv->sliding_key_timeout && (pressed_key->key_type & KEY_TYPE_SLIDING) )
    {
      priv->sliding_key = pressed_key;
      hildon_vkb_renderer_input_slide(self, 0);

      if ( priv->pressed_key->current_slide_key >= priv->pressed_key->byte_count - 1 )
        priv->pressed_key->current_slide_key = 0;
      else
        priv->pressed_key->current_slide_key++;

      priv->pressed_key->width = 0;
      priv->sliding_key_timer = g_timeout_add(
                                  priv->sliding_key_timeout,
                                  hildon_vkb_renderer_sliding_key_timeout,
                                  widget);
      b = 1;
    }
    else
    {
      hildon_vkb_renderer_input_key(self);
      b = 1;
    }
  }
  timer = priv->key_repeat_timer;
  if ( timer )
    goto LABEL_14;
  pressed_key = priv->pressed_key;
LABEL_15:
  if ( pressed_key )
  {
    dead_key = priv->dead_key;
    if ( dead_key && dead_key != pressed_key )
    {
      key_flags = pressed_key->key_flags;
      shift = pressed_key->key_flags & KEY_TYPE_SHIFT;
      if ( pressed_key->key_flags & KEY_TYPE_SHIFT )
      {
LABEL_22:
        if ( shift == KEY_TYPE_SHIFT && b )
        {
          hildon_vkb_renderer_key_update(self,
                                         pressed_key,
                                         (priv->shift_active = (!priv->shift_active)),
                                         1);
          hildon_vkb_renderer_input_key(self);
        }
        else
        {
          if ( key_flags & KEY_TYPE_DEAD && b )
          {
            if ( priv->dead_key == pressed_key )
            {
              hildon_vkb_renderer_key_update(self, pressed_key, 0, 1);
              if ( priv->dead_key )
              {
                g_signal_emit(self, signals[COMBINING_INPUT], 0, 0, 0);
                hildon_vkb_renderer_input_key(self);
              }
              priv->dead_key = 0;
            }
            else
            {
              hildon_vkb_renderer_key_update(self, pressed_key, 1, 1);

              priv->dead_key = priv->pressed_key;
              g_signal_emit(self, signals[COMBINING_INPUT], 0,
                            priv->pressed_key->labels, 0);
            }
          }
          else
          {
            if ( !is_shift_or_dead_key(pressed_key) )
            {
              hildon_vkb_renderer_key_update(self, pressed_key, 0, 1);
              g_signal_emit(self, signals[TEMP_INPUT], 0, 0, 1);
            }
          }
        }
        goto out;
      }
      if ( !priv->field_20 )
      {
        hildon_vkb_renderer_key_update(self, dead_key, 0, 1);
        pressed_key = priv->pressed_key;
      }
      priv->dead_key = 0;
    }
    key_flags = pressed_key->key_flags;
    shift = pressed_key->key_flags & KEY_TYPE_SHIFT;
    goto LABEL_22;
  }

out:
  hildon_vkb_renderer_release_cleanup(priv);
  return TRUE;
}

static void
get_key_from_coordinates(HildonVKBRenderer *self,GdkEventMotion *event)
{
  int i,j;
  vkb_key* key;
  HildonVKBRendererPrivate *priv;
  tracef;

  g_return_if_fail(self);

  priv=HILDON_VKB_RENDERER_GET_PRIVATE(self);

  if ( priv->sub_layout )
  {
    if ( priv->sub_layout->num_key_sections )
    {
      i = 0;
      while ( 1 )
      {
        if ( priv->sub_layout->key_sections[i].num_keys )
          break;
LABEL_21:
        i++;
        if ( priv->sub_layout->num_key_sections == i )
          goto out;
      }

      j = 0;

      while ( 1 )
      {
        key = &priv->sub_layout->key_sections[i].keys[j];

        if ( event->x > key->left &&
             event->x <= key->right &&
             event->y > key->top &&
             event->y <= key->bottom )
          break;
        ++j;

        if ( j == priv->sub_layout->key_sections[i].num_keys )
          goto LABEL_21;
      }
      if ( key->gtk_state != 4 )
      {
        if ( key != priv->pressed_key )
        {
          if ( !priv->key_repeat_timer )
          {
            hildon_vkb_renderer_key_update(HILDON_VKB_RENDERER(self), priv->pressed_key, 0, 1);
            priv->pressed_key = key;
            hildon_vkb_renderer_key_update(HILDON_VKB_RENDERER(self), key, 1, 1);
            if ( !(priv->pressed_key->key_type & 1) )
              g_signal_emit(HILDON_VKB_RENDERER(self), signals[TEMP_INPUT], 0,
                            priv->pressed_key->labels, 1);
          }
        }
      }
      return;
    }
  }
out:
  if ( priv->pressed_key )
  {
    if ( priv->pressed_key->gtk_state != 4 )
    {
      hildon_vkb_renderer_key_update(HILDON_VKB_RENDERER(self), priv->pressed_key, 0, 1);

      if ( !(priv->pressed_key->key_type & 1) )
        g_signal_emit(HILDON_VKB_RENDERER(self), signals[TEMP_INPUT], 0, 0, 1);
    }
  }
}

static gint
hildon_vkb_renderer_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
  int diff_x;
  int diff_y;
  HildonVKBRendererPrivate *priv;
tracef;
  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(widget),1);

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(HILDON_VKB_RENDERER(widget));

  if ( !priv->pressed_key )
    return 0;
  if(priv->pressed_key->left >= event->x ||
     event->x >= priv->pressed_key->right ||
     priv->pressed_key->top >= event->y ||
     event->y >= priv->pressed_key->bottom)
  {
    if ( priv->key_repeat_init_timer )
    {
      g_source_remove(priv->key_repeat_init_timer);
      priv->key_repeat_init_timer = 0;
    }
    if ( priv->key_repeat_timer )
    {
      g_source_remove(priv->key_repeat_timer);
      priv->key_repeat_timer = 0;
      priv->field_58 = FALSE;
    }
    priv->sliding_key = 0;

    diff_x = (event->x - priv->x);
    if ( diff_x < 0 )
      diff_x = -diff_x;

    diff_y = (event->y - priv->y);
    if ( diff_y < 0 )
      diff_y = -diff_y;
    if(diff_x > 50 || diff_y > 50)
    {
      if ( !priv->field_C0 )
      {
        priv->field_C0 = TRUE;
        hildon_vkb_renderer_input_key(HILDON_VKB_RENDERER(widget));
      }
      priv->field_58 = FALSE;
    }
  }

  get_key_from_coordinates(HILDON_VKB_RENDERER(widget),event);

  if ( priv->field_58 )
  {
    priv->x = event->x;
    priv->y = event->y;
  }

  return 1;
}

static gboolean
hildon_vkb_renderer_expose(GtkWidget *widget, GdkEventExpose *event)
{
  HildonVKBRenderer *self;
  vkb_sub_layout *sub_layout;
  vkb_key_section *key_section;
  vkb_key *key;
  GtkStyle *style;
  PangoLayoutIter *iter;
  unsigned int j;
  cairo_t *ct;
  gchar *text;
  gchar *tmp_text;
  GdkRectangle src;
  GdkRectangle dest;
  int height;
  int i;
  int n_rectangles;
  GdkRectangle *rectangles;

  HildonVKBRendererPrivate *priv;
tracef;
  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(widget),FALSE);

  self = HILDON_VKB_RENDERER(widget);

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);

  if(!priv->sub_layout && !hildon_vkb_renderer_load_layout(self, 1))
    if ( !priv->sub_layout && priv->pixmap )
    return FALSE;

  if ( priv->field_28 )
    hildon_vkb_renderer_paint_pixmap(self);
  gdk_region_get_rectangles(event->region, &rectangles, &n_rectangles);
  i = 0;
  if ( n_rectangles > 0 )
  {
    do
    {
      GdkRectangle *r = &rectangles[i];
      gdk_draw_drawable(
        widget->window,
        widget->style->bg_gc[0],
        priv->pixmap,
        r->x,
        r->y,
        r->x,
        r->y,
        r->width,
        r->height);
      i++;
    }
    while ( n_rectangles > i );
  }
  g_free(rectangles);

  ct = gdk_cairo_create(widget->window);
  cairo_rectangle(
    ct,
    event->area.x,
    event->area.y,
    event->area.width,
    event->area.height);
  cairo_clip(ct);
  pango_layout_set_text(priv->pango_layout, "kq", 1);
  pango_layout_get_pixel_size(priv->pango_layout, &i, &height);
  i = 0;
  sub_layout = priv->sub_layout;
  if ( sub_layout->num_key_sections )
  {
    while ( 1 )
    {
      key_section = &sub_layout->key_sections[i];
      if ( key_section->num_keys )
        break;
LABEL_42:
      i++;
      if ( sub_layout->num_key_sections <= i )
        goto out;
    }
    j = 0;
    j = 0;
    while ( 1 )
    {
      if ( priv->secondary_layout )
      {
        key = &key_section->keys[j];
        if ( key->num_sub_keys )
          key = key->sub_keys;
      }
      else
      {
        key = &key_section->keys[j];
      }
      src.x = key->left;
      src.y = key->top;
      src.width = key->right - key->left;
      src.height = key->bottom - key->top;
      if ( !gdk_rectangle_intersect(&event->area, &src, &dest) )
        goto LABEL_28;
      if ( key->key_flags & KEY_TYPE_DEAD )
        style = priv->special.gtk_style;
      else
        style = priv->normal.gtk_style;
      gdk_cairo_set_source_color(ct, &style->fg[key->gtk_state]);

      if(key->key_type & KEY_TYPE_SLIDING)
      {
        gchar **labels = key->labels;
        unsigned char byte_count = key->byte_count;

        if ( byte_count > 2 )
        {
          int next = 2,
              current = 1,
              prev = 0;

          if (key->current_slide_key)
          {
            next = key->current_slide_key + 1;
            current = key->current_slide_key;
            prev = key->current_slide_key - 1;
          }
          else if ( priv->sliding_key_timer )
          {
            next = byte_count + 1;
            current = byte_count;
            prev = byte_count - 1;
          }

          tmp_text = g_strconcat(
                       labels[prev % byte_count],
                       " ",
                       labels[current % byte_count],
                       " ",
                       labels[next % byte_count],
                       NULL);

          text = tmp_text;
        }
        else
        {
          text = labels[key->current_slide_key];
          tmp_text = NULL;
        }
      }
      else if ((key->key_type & KEY_TYPE_MULTIPLE) && key->sub_keys && key->num_sub_keys > 1)
      {
        if ( priv->secondary_layout )
        {
          text = (gchar *)key->sub_keys[1].labels;
          tmp_text = 0;
        }
        else
        {
          text = (gchar *)key->sub_keys->labels;
          tmp_text = 0;
        }
      }
      else
      {
        text = (gchar *)key->labels;
        tmp_text = 0;
      }

      pango_layout_set_text(priv->pango_layout, text, strlen(text));
      pango_layout_get_pixel_size(priv->pango_layout, (int*)&key->width, 0);

      iter = pango_layout_get_iter(priv->pango_layout);
      key->offset = height / 2 + (signed int)(key->bottom - key->top) / 2;
      pango_layout_iter_free(iter);

      if ( (key->key_type & KEY_TYPE_SLIDING) && key->byte_count > 2 )
      {
        cairo_move_to(ct,
                      key->left + (key->right - key->left - key->width)/2,
                      key->top + (key->bottom - key->top - key->offset)/2);

        cairo_save(ct);
        gdk_cairo_set_source_color(ct, &priv->slide.gtk_style->fg[4]);
        pango_cairo_show_layout(ct, priv->pango_layout);
        cairo_restore(ct);

        if ( priv->sliding_key_timer )
        {
          pango_layout_set_text(priv->pango_layout, text, g_utf8_next_char(text)-text);
          pango_cairo_show_layout(ct, priv->pango_layout);
        }
      }
      else
      {
        cairo_move_to(
          ct,
          (key->left + ((key->right - key->left - key->width)/2)),
          (key->top + (key->bottom - key->top - key->offset)/2));
        pango_cairo_show_layout(ct, priv->pango_layout);
      }

      g_free(tmp_text);
LABEL_28:
      j++;
      sub_layout = priv->sub_layout;
      key_section = &sub_layout->key_sections[i];
      if ( j >= key_section->num_keys )
        goto LABEL_42;
    }
  }
out:
  cairo_destroy(ct);
  return TRUE;
}

static gboolean
hildon_vkb_renderer_button_press(GtkWidget *widget, GdkEventButton *event)
{
  vkb_key_section *section;
  int i,j;
  HildonVKBRendererPrivate *priv;
  unsigned int num_keys;
  tracef;

  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(widget),TRUE);

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(HILDON_VKB_RENDERER(widget));

  if ( event->button == 1 && priv->sub_layout )
  {
    if ( priv->key_repeat_init_timer )
    {
      g_source_remove(priv->key_repeat_init_timer);
      priv->key_repeat_init_timer = 0;
    }
    if ( priv->key_repeat_timer )
    {
      g_source_remove(priv->key_repeat_timer);
      priv->key_repeat_timer = 0;
    }
    if ( priv->sliding_key_timer )
    {
      g_source_remove(priv->sliding_key_timer);
      priv->sliding_key_timer = 0;
    }

    if ( priv->sub_layout->num_key_sections )
    {
      int section_index = 0;
      section = priv->sub_layout->key_sections;
      num_keys = section->num_keys;
      if ( !num_keys )
        goto LABEL_22;

LABEL_12:
      i = 0;
      j = 0;
      while ( 1 )
      {
        vkb_key *key = &section->keys[j];
        vkb_key *tmp_key;

        if ( event->x <= key->left  || key->right < event->x )
          goto LABEL_14;
        if ( event->y > key->top )
        {
          if ( key->bottom >= event->y )
          {
            if ( key->gtk_state == 4 )
            {
              g_signal_emit(widget, signals[ILLEGAL_INPUT], 0,
                            key->key_type & KEY_TYPE_SLIDING ?
                              key->labels[0] : (gchar *)key->labels,
                            0);
              return TRUE;
            }

            priv->pressed_key = key;

            if ( !(key->key_type & KEY_TYPE_SLIDING) && !(key->key_flags & KEY_TYPE_SHIFT) )
              g_signal_emit(HILDON_VKB_RENDERER(widget), signals[TEMP_INPUT], 0,
                            key->labels, 1);

            if ( priv->sliding_key )
            {
              tmp_key = priv->pressed_key;
              if ( priv->sliding_key != tmp_key )
              {
                hildon_vkb_renderer_input_slide(HILDON_VKB_RENDERER(widget), 1);
                tmp_key = priv->pressed_key;
              }
            }
            else
              tmp_key = priv->pressed_key;

            if ( !is_shift_or_dead_key(tmp_key) )
            {
              hildon_vkb_renderer_key_update(HILDON_VKB_RENDERER(widget), tmp_key, 1, 1);
              tmp_key = priv->pressed_key;
            }

            if ( priv->key_repeat_interval )
            {
              if ( !tmp_key->key_type )
              {
                if ( !((tmp_key->key_flags>>8) & 2) && !is_shift_or_dead_key(tmp_key) )
                {
                  priv->key_repeat_init_timer = g_timeout_add(
                                     priv->key_repeat_interval + 625,
                                     hildon_vkb_renderer_key_repeat_init,
                                     widget);
                  tmp_key = priv->pressed_key;
                  goto LABEL_38;
                }
LABEL_35:
                priv->x = event->x;
                priv->y = event->y;
                goto LABEL_19;
              }
            }
            else
            {
LABEL_38:
              if ( !tmp_key->key_type )
                goto LABEL_35;
            }
            priv->field_58 = FALSE;
            goto LABEL_19;
          }
LABEL_14:
          i++;
          j = i;
          if ( i == num_keys )
          {
LABEL_19:
            while ( 1 )
            {
              if ( j != priv->sub_layout->key_sections[section_index].num_keys )
                return 1;
              section_index++;
              if ( priv->sub_layout->num_key_sections <= section_index )
                return 1;
              section = &priv->sub_layout->key_sections[section_index];
              num_keys = priv->sub_layout->key_sections[section_index].num_keys;
              if ( num_keys )
                goto LABEL_12;
LABEL_22:
              j = 0;
            }
          }
        }
        else
        {
          i++;
          j = i;
          if ( i == num_keys )
            goto LABEL_19;
        }
      }
    }
  }
  return 1;
}

static void
hildon_vkb_renderer_class_init (HildonVKBRendererClass *klass)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  tracef;

  object_class = G_OBJECT_CLASS(klass);
  widget_class = GTK_WIDGET_CLASS(klass);

  object_class->set_property  = hildon_vkb_renderer_set_property;
  object_class->get_property  = hildon_vkb_renderer_get_property;
  object_class->finalize      = hildon_vkb_renderer_finalize;

  widget_class->realize             = hildon_vkb_renderer_realize;
  widget_class->unrealize           = hildon_vkb_renderer_unrealize;
  widget_class->expose_event        = hildon_vkb_renderer_expose;
  widget_class->size_allocate       = hildon_vkb_renderer_size_allocate;
  widget_class->button_press_event  = hildon_vkb_renderer_button_press;
  widget_class->button_release_event= hildon_vkb_renderer_button_release;
  widget_class->motion_notify_event = hildon_vkb_renderer_motion_notify;

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_DIMENSION,
                                  g_param_spec_boxed(
                                    "dimension",
                                    "max dimension of the pixmap",
                                    "GtkReqisition defining the size of the pixmap",
                                    GTK_TYPE_REQUISITION,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)
                                  );

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_COLLECTION_FILE,
                                  g_param_spec_string(
                                    "collection",
                                    "collection file name",
                                    "the file name of the layout collection to be used",
                                    NULL,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_LAYOUT,
                                  g_param_spec_int(
                                    "layout",
                                    "current active layout",
                                    "currently active layout indicated by layout type",
                                    0,
                                    8,
                                    0,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_SUBLAYOUT,
                                  g_param_spec_int(
                                    "sub",
                                    "current active sublayout",
                                    "currently active sublayout indicated by sublayout index",
                                    0,
                                    2147483647,
                                    0,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_LAYOUTS,
                                  g_param_spec_pointer(
                                    "layouts",
                                    "layouts inside a collection",
                                    "tells all the layouts this current collection have",
                                    G_PARAM_READABLE));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_SUBLAYOUTS,
                                  g_param_spec_pointer(
                                    "subs",
                                    "sublayout inside a layout",
                                    "tells all the sublayout this widget can activate",
                                    G_PARAM_READABLE));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_MODE,
                                  g_param_spec_int(
                                    "mode",
                                    "mode bitmask",
                                    "enable/disable mode for this layout",
                                    1,
                                    8191,
                                    8191,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_OPTION,
                                  g_param_spec_boolean(
                                    "option",
                                    "secondary option layout",
                                    "turn on/off secondary layout with secondary layout",
                                    0,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_STYLE_NORMAL,
                                  g_param_spec_string(
                                    "style_normal",
                                    "normal style path",
                                    "string defines widget path of the style for normal keys",
                                    ".osso-im-key",
                                    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_STYLE_SPECIAL,
                                  g_param_spec_string(
                                    "style_special",
                                    "special style path",
                                    "string defines widget path of the style for special keys",
                                    ".osso-im-accent",
                                    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_STYLE_SLIDE,
                                  g_param_spec_string(
                                    "style_slide",
                                    "slide style path",
                                    "string defines widget path of the style for sliding keys",
                                    ".osso-im-key",
                                    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_STYLE_WHITESPACE,
                                  g_param_spec_string(
                                    "style_whitespace",
                                    "whitespace style path",
                                    "string defines widget path of the style for whitespace keys",
                                    ".osso-im-key",
                                    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_STYLE_TAB,
                                  g_param_spec_string(
                                    "style_tab",
                                    "Tab style path",
                                    "string defines widget path of the style for TAB key",
                                    ".osso-im-key",
                                    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_STYLE_BACKSPACE,
                                  g_param_spec_string(
                                    "style_backspace",
                                    "backspace style path",
                                    "string defines widget path of the style for backspace keys",
                                    ".osso-im-key",
                                    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_STYLE_SHIFT,
                                  g_param_spec_string(
                                    "style_shift",
                                    "shift style path",
                                    "string defines widget path of the style for shift keys",
                                    ".osso-im-key",
                                    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_REPEAT_INTERVAL,
                                  g_param_spec_int(
                                    "repeat_interval",
                                    "repeat rate in ms",
                                    "set length of the key repeating rate",
                                    0,
                                    2147483647,
                                    0,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_CYCLE_TIMEOUT,
                                  g_param_spec_int(
                                    "cycle_timeout",
                                    "the timeout for sliding key in ms",
                                    "set the timeout for finalized slidingkey input",
                                    0,
                                    2147483647,
                                    750,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_WC_LANGUAGE,
                                  g_param_spec_string(
                                    "WC_language",
                                    "word completion language",
                                    "string tells the defined word completion language for current collection",
                                    0,
                                    G_PARAM_READABLE));

  g_object_class_install_property(object_class,
                                  HILDON_VKB_RENDERER_PROP_GESTURE_RANGE,
                                  g_param_spec_int(
                                    "gesture_range",
                                    "the gesture recognition range threshold in px",
                                    "set the range for gestures",
                                    0,
                                    2147483647,
                                    20,
                                    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  signals[INPUT] = g_signal_new("input",
                            HILDON_VKB_RENDERER_TYPE,
                            G_SIGNAL_RUN_LAST,
                            G_STRUCT_OFFSET(HildonVKBRendererClass,input),
                            NULL, NULL,
                            hildon_vkb_renderer_marshal_VOID__STRING_BOOLEAN,
                            G_TYPE_NONE,
                            2, G_TYPE_STRING, G_TYPE_BOOLEAN);

  signals[ILLEGAL_INPUT] = g_signal_new("illegal_input",
                            HILDON_VKB_RENDERER_TYPE,
                            G_SIGNAL_RUN_LAST,
                            G_STRUCT_OFFSET(HildonVKBRendererClass,illegal_input),
                            NULL, NULL,
                            &g_cclosure_marshal_VOID__STRING,
                            G_TYPE_NONE,
                            1, G_TYPE_STRING);

  signals[COMBINING_INPUT] = g_signal_new("combining_input",
                            HILDON_VKB_RENDERER_TYPE, G_SIGNAL_RUN_LAST,
                            G_STRUCT_OFFSET(HildonVKBRendererClass,illegal_input),
                            NULL, NULL,
                            &g_cclosure_marshal_VOID__STRING,
                            G_TYPE_NONE,
                            1, G_TYPE_STRING);

  signals[TEMP_INPUT] = g_signal_new("temp-input",
                            HILDON_VKB_RENDERER_TYPE,
                            G_SIGNAL_RUN_LAST,
                            G_STRUCT_OFFSET(HildonVKBRendererClass,input),
                            NULL, NULL,
                            hildon_vkb_renderer_marshal_VOID__STRING_BOOLEAN,
                            G_TYPE_NONE,
                            2, G_TYPE_STRING, G_TYPE_BOOLEAN);
}

static void
hildon_vkb_renderer_class_finalize(HildonVKBRendererClass *klass)
{
}

static void
hildon_vkb_renderer_key_update(HildonVKBRenderer *self, vkb_key *key, int shift_active, int repaint)
{
  HildonVKBRendererPrivate *priv;
  GtkStyle *style;
  vkb_key *tmp_key;
  tracef;

  g_return_if_fail(HILDON_IS_VKB_RENDERER(self));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);

  if((key->key_type & KEY_TYPE_HEXA) && (key->sub_keys) && (key->num_sub_keys > 1))
  {
    if ( priv->secondary_layout )
      tmp_key = key->sub_keys + 1;
    else
      tmp_key = key->sub_keys;
  }
  else
    tmp_key = key;

  if(shift_active >= 0)
    tmp_key->gtk_state = shift_active;

  if(key->key_type & KEY_TYPE_SLIDING)
    style = priv->slide.gtk_style;
  else if(key->key_flags & KEY_TYPE_WHITESPACE)
    style = priv->whitespace.gtk_style;
  else if((key->key_flags & KEY_TYPE_TAB) == KEY_TYPE_TAB)
    style = priv->tab.gtk_style;
  else if((key->key_flags & KEY_TYPE_BACKSPACE) == KEY_TYPE_BACKSPACE)
    style = priv->backspace.gtk_style;
  else if((key->key_flags & KEY_TYPE_SHIFT) == KEY_TYPE_SHIFT)
    style = priv->shift.gtk_style;
  else if(key->key_flags & KEY_TYPE_DEAD)
    style = priv->special.gtk_style;
  else
    style = priv->normal.gtk_style;

  if(GTK_IS_STYLE(style))
  {
    style->depth = gdk_drawable_get_depth(self->parent.window);
    gtk_paint_box(style, priv->pixmap, tmp_key->gtk_state, 1, 0, GTK_WIDGET(self), 0,
                  key->left, key->top, key->right - key->left, key->bottom - key->top);

    if ( repaint )
      gtk_widget_queue_draw_area(GTK_WIDGET(self), key->left, key->top, key->right - key->left, key->bottom - key->top);
  }
}

void
hildon_vkb_renderer_set_sublayout_type(HildonVKBRenderer *renderer, unsigned int sub_layout_type)
{
  HildonVKBRendererPrivate *priv;
  vkb_sub_layout *sub_layout;
  int i=0;
  tracef;

  g_return_if_fail(HILDON_IS_VKB_RENDERER(renderer));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(renderer);

  if(priv->layout)
  {
    if(priv->sub_layout && priv->sub_layout->type == sub_layout_type)
      return;
  }
  else
  {
    if(!hildon_vkb_renderer_load_layout(renderer, 0))
      return;
  }

  if(priv->layout)
  {
    if(priv->layout->sub_layouts->type == sub_layout_type)
      g_object_set(renderer, "sub", 0, NULL);
    else
    {
      sub_layout = priv->layout->sub_layouts;
      while(1)
      {
        i++;
        if(i == priv->layout->num_sub_layouts)
          break;
        sub_layout++;
        if(sub_layout->type == sub_layout_type)
        {
          g_object_set(renderer, "sub", i, NULL);
          break;
        }
      }
    }
  }

  if(!priv->sub_layout)
    hildon_vkb_renderer_load_layout(renderer, 1);

  if(priv->layout->num_sub_layouts != i)
    gtk_widget_queue_draw(GTK_WIDGET(renderer));
}

void
hildon_vkb_renderer_clear_dead_key(HildonVKBRenderer *renderer)
{
  HildonVKBRendererPrivate *priv;
  tracef;

  g_return_if_fail(HILDON_IS_VKB_RENDERER(renderer));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(renderer);

  if(priv->dead_key)
  {
    if(priv->field_20)
      priv->field_20 = FALSE;
    else
      hildon_vkb_renderer_key_update(renderer, priv->dead_key, 0, 1);
    priv->dead_key = 0;
    g_signal_emit(renderer, signals[COMBINING_INPUT], 0, 0, 0);
  }
}

gchar*
hildon_vkb_renderer_get_dead_key(HildonVKBRenderer *renderer)
{
  HildonVKBRendererPrivate *priv;

  tracef;

  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(renderer),NULL);

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(renderer);

  if(priv->dead_key)
    return g_strdup((gchar *)priv->dead_key->labels);

  return NULL;
}

static void
hildon_vkb_renderer_input_key(HildonVKBRenderer *self)
{
  HildonVKBRendererPrivate *priv;
  vkb_key *key;
  tracef;

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);
  key = priv->pressed_key;
  if ( key )
  {
    if(priv->dead_key)
      hildon_vkb_renderer_accent_combine_input(self, (gchar *)key->labels, 1);
    else
      g_signal_emit(self, signals[INPUT], 0, key->labels, 1);
  }
}

gboolean
hildon_vkb_renderer_key_repeat(void *data)
{
  HildonVKBRenderer *renderer;
  HildonVKBRendererPrivate *priv;
  tracef;

  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(data),FALSE);

  renderer = HILDON_VKB_RENDERER(data);
  priv = HILDON_VKB_RENDERER_GET_PRIVATE(renderer);

  if(gdk_window_is_viewable(GTK_WIDGET(renderer)->window) &&
     (gdk_window_get_state(GTK_WIDGET(renderer)->window) != GDK_WINDOW_STATE_BELOW))
  {
    hildon_vkb_renderer_input_key(renderer);
    return TRUE;
  }

  hildon_vkb_renderer_key_update(renderer, priv->pressed_key, 0, 1);
  hildon_vkb_renderer_release_cleanup(priv);
  priv->key_repeat_timer = 0;

  return FALSE;
}

static gboolean
hildon_vkb_renderer_key_repeat_init(void *data)
{
  HildonVKBRendererPrivate *priv;
  tracef;

  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(data),FALSE);

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(HILDON_VKB_RENDERER(data));

  if(priv->pressed_key)
  {
    priv->key_repeat_timer = g_timeout_add(
                               priv->key_repeat_interval,
                               hildon_vkb_renderer_key_repeat,
                               data);

    hildon_vkb_renderer_input_key(HILDON_VKB_RENDERER(data));
    priv->key_repeat_init_timer = 0;
  }

  return FALSE;
}


static gboolean
hildon_vkb_renderer_sliding_key_timeout(void *data)
{
  tracef;

  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(data),FALSE);

  hildon_vkb_renderer_input_slide(HILDON_VKB_RENDERER(data), 1);

  return FALSE;
}
