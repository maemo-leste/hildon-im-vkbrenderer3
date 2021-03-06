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
signals[LAST_SIGNAL] = { 0 };


#if 0
#define tracef g_warning("%s\n",__func__);
#else
#define tracef
#endif

typedef struct {
 gchar *style;
 GtkStyle *gtk_style;
} HildonVKBRendererKeyStyle;

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
  gboolean sub_layout_changed;
  gboolean secondary_layout;
  gboolean paint_pixmap_pending;
  PangoLayout *pango_layout;
  GdkPixmap *pixmap;
  GtkRequisition requisition;
  vkb_key *pressed_key;
  vkb_key *dead_key;
  gdouble x;
  gdouble y;
  gboolean track_position;
  int gesture_range;
  vkb_key *sliding_key;
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
  gboolean max_distance_exceeded;
  gboolean shift_active;
};

#define HILDON_VKB_RENDERER_GET_PRIVATE(renderer) \
  ((HildonVKBRendererPrivate *)hildon_vkb_renderer_get_instance_private(renderer))

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
  HildonVKBRenderer, hildon_vkb_renderer, GTK_TYPE_WIDGET, 0,
  G_ADD_PRIVATE_DYNAMIC(HildonVKBRenderer)
);

#define KEY_TYPE_ALL \
  (KEY_TYPE_ALPHA | KEY_TYPE_NUMERIC | KEY_TYPE_HEXA | \
  KEY_TYPE_TELE | KEY_TYPE_SPECIAL | KEY_TYPE_DEAD | KEY_TYPE_WHITESPACE | \
  KEY_TYPE_EXTRA_BYTE | KEY_TYPE_RAW | KEY_TYPE_TONE | KEY_TYPE_TAB | \
  KEY_TYPE_BACKSPACE | KEY_TYPE_SHIFT)

static void hildon_vkb_renderer_key_update(HildonVKBRenderer *self, vkb_key *key, gint state, gboolean repaint);
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

  if (priv->sliding_key)
    hildon_vkb_renderer_input_slide(self, TRUE);

  if (priv->sliding_key_timer)
  {
    g_source_remove(priv->sliding_key_timer);
    priv->sliding_key_timer = 0;
  }
}

static void
hildon_vkb_renderer_update_mode(HildonVKBRenderer *self, gboolean update_keys)
{
  HildonVKBRendererPrivate *priv;
  vkb_sub_layout *sub_layout;
  int i = 0;

  tracef;
  g_return_if_fail (HILDON_IS_VKB_RENDERER (self));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);

  sub_layout = priv->sub_layout;

  if (!sub_layout)
    return;

  for (i = 0; i < sub_layout->num_key_sections; i++)
  {
    vkb_key_section *section = &sub_layout->key_sections[i];
    int j;

    for (j = 0; j < section->num_keys; j++)
    {
      vkb_key *key = &section->keys[j];

      if ((key->key_type & KEY_TYPE_HEXA) && key->sub_keys &&
          key->num_sub_keys > 1)
      {
        GtkStateType skey_state;
        int k;

        if (priv->secondary_layout)
          skey_state = key->sub_keys[1].gtk_state;
        else
          skey_state = key->sub_keys->gtk_state;

        for (k = 0; k < key->num_sub_keys; k++)
        {
          vkb_key *skey = &key->sub_keys[k];

          if (skey->key_flags & priv->mode_bitmask &&
              skey->labels && *skey->labels)
          {
            skey->gtk_state = GTK_STATE_NORMAL;
          }
          else
            skey->gtk_state = GTK_STATE_INSENSITIVE;
        }

        if (priv->secondary_layout)
        {
          if (key->sub_keys[1].gtk_state == skey_state)
            continue;
        }
        else if (key->sub_keys->gtk_state == skey_state)
            continue;
      }
      else
      {
        GtkStateType key_state = key->gtk_state;
        int flags = key->key_flags & priv->mode_bitmask;

        if ((flags && (key->labels &&
                       (key->key_type == KEY_TYPE_SLIDING || *key->labels))) ||
            (flags & (KEY_TYPE_TAB | KEY_TYPE_WHITESPACE)) ||
            (key->key_flags & KEY_TYPE_SHIFT))
        {
          key->gtk_state = GTK_STATE_NORMAL;
        }
        else
          key->gtk_state = GTK_STATE_INSENSITIVE;

        if (key->gtk_state == key_state)
          continue;
      }

      if (update_keys)
        hildon_vkb_renderer_key_update(self, key, -1, TRUE);
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
  hildon_vkb_renderer_update_mode(self, FALSE);
  hildon_vkb_renderer_paint_pixmap(self);
}

static gboolean
hildon_vkb_renderer_load_layout(HildonVKBRenderer *self,
                                gboolean load_sub_layout)
{
  HildonVKBRendererPrivate *priv;

  tracef;
  g_return_val_if_fail (HILDON_IS_VKB_RENDERER (self), FALSE);

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);

  if (!priv->layout_collection)
    return FALSE;

  if (!priv->layout)
  {
    priv->layout = imlayout_vkb_get_layout(priv->layout_collection,
                                           priv->layout_type);

    if (!priv->layout)
    {
      g_warning("Set layout type does not exist in current collection,"
                " will load default layout in current collection\n");

      priv->layout =
          imlayout_vkb_get_layout(priv->layout_collection,
                                  *priv->layout_collection->layout_types);

      if (!priv->layout)
        goto error;
    }
  }

  if (!load_sub_layout || priv->sub_layout)
    return TRUE;

  hildon_vkb_renderer_clear(self);

  if (priv->num_sub_layouts >= priv->layout->num_sub_layouts)
  {
    g_warning("Set sublayout type does not exist in current layout,"
              " will load default sublayout in current layout\n");
    priv->sub_layout = priv->layout->sub_layouts;
    priv->num_sub_layouts = 0;
  }
  else
    priv->sub_layout = &priv->layout->sub_layouts[priv->num_sub_layouts];

  if (priv->sub_layout)
  {
    hildon_vkb_renderer_update_pixmap(self);

    if (priv->WC_language)
    {
      imlayout_vkb_free_layout(priv->WC_language);
      priv->WC_language = NULL;
    }

    priv->current_sub_layout = NULL;
    return TRUE;
  }

error:
    g_warning("Loading default layout fails\n");

  return FALSE;
}

guint
hildon_vkb_renderer_get_numeric_sub(HildonVKBRenderer *renderer)
{
  HildonVKBRendererPrivate *priv;

  tracef;
  g_return_val_if_fail(HILDON_IS_VKB_RENDERER (renderer), -1);

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(renderer);

  if (!priv->layout)
  {
    hildon_vkb_renderer_load_layout(renderer, TRUE);

    if (!priv->layout)
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
    hildon_vkb_renderer_load_layout(renderer, TRUE);

  if (priv->sub_layout && !(priv->sub_layout->variance_index & 0x80))
  {
    g_object_set(renderer, "sub", priv->sub_layout->variance_index, NULL);
    gtk_widget_queue_draw(GTK_WIDGET(renderer));
  }
}

static void
update_style(HildonVKBRenderer *self, const gchar *new_style, guint prop_id,
             gboolean keep)
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

  if (!keep)
  {
    if(key_style->style)
      g_free(key_style->style);

    key_style->style = g_strdup(new_style);
  }

  key_style->gtk_style =
      gtk_rc_get_style_by_paths(gtk_widget_get_settings(
                                  GTK_WIDGET(self)),
                                  key_style->style, NULL, GTK_TYPE_BUTTON);

  if (!key_style->gtk_style)
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

  if (priv->font_desc)
    pango_font_description_free(priv->font_desc);

  priv->font_desc = pango_font_description_copy(widget->style->font_desc);
  pango_font_description_set_size(
        priv->font_desc,
        1.2 * pango_font_description_get_size(priv->font_desc));

  if (priv->pango_layout)
    pango_layout_set_font_description(priv->pango_layout, priv->font_desc);

  update_style(HILDON_VKB_RENDERER(widget), priv->normal.style,
               HILDON_VKB_RENDERER_PROP_STYLE_NORMAL, TRUE);
  update_style(HILDON_VKB_RENDERER(widget), priv->special.style,
               HILDON_VKB_RENDERER_PROP_STYLE_SPECIAL, TRUE);
  update_style(HILDON_VKB_RENDERER(widget), priv->slide.style,
               HILDON_VKB_RENDERER_PROP_STYLE_SLIDE, TRUE);
  update_style(HILDON_VKB_RENDERER(widget), priv->whitespace.style,
               HILDON_VKB_RENDERER_PROP_STYLE_WHITESPACE, TRUE);
  update_style(HILDON_VKB_RENDERER(widget), priv->tab.style,
               HILDON_VKB_RENDERER_PROP_STYLE_TAB, TRUE);
  update_style(HILDON_VKB_RENDERER(widget), priv->backspace.style,
               HILDON_VKB_RENDERER_PROP_STYLE_BACKSPACE, TRUE);
  update_style(HILDON_VKB_RENDERER(widget), priv->shift.style,
               HILDON_VKB_RENDERER_PROP_STYLE_SHIFT, TRUE);
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
  priv->current_sub_layout = NULL;
  priv->mode_bitmask = 0;
  priv->layout_type = 0;
  priv->sub_layout_changed = FALSE;
  priv->paint_pixmap_pending = FALSE;
  priv->pixmap = NULL;
  priv->pressed_key = NULL;
  priv->x = 0;
  priv->y = 0;
  priv->track_position = TRUE;
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
  priv->max_distance_exceeded = FALSE;
  priv->shift_active = FALSE;

  g_signal_connect(self, "style-set", G_CALLBACK(theme_changed), self);
}

gboolean
hildon_vkb_renderer_get_shift_active(HildonVKBRenderer * renderer)
{
  tracef;
  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(renderer), FALSE);

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
  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(renderer), 0);

  return HILDON_VKB_RENDERER_GET_PRIVATE(renderer)->pressed_key->key_flags;
}

static void
hildon_vkb_renderer_release_cleanup(HildonVKBRendererPrivate *priv)
{
  g_return_if_fail(priv != NULL);

  priv->pressed_key = NULL;
  priv->x = 0;
  priv->y = 0;
  priv->track_position = TRUE;
  priv->sub_layout_changed = FALSE;
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
hildon_vkb_renderer_get_property(GObject *object, guint prop_id, GValue *value,
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
      if (!priv->layout_collection)
        g_value_set_string(value, NULL);
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
      if (!priv->layout &&
          !hildon_vkb_renderer_load_layout(HILDON_VKB_RENDERER(object), TRUE))
      {
        g_value_set_pointer(value, NULL);
        break;
      }
      else
      {
        HildonVKBRendererLayoutInfo *li =
            g_new0(HildonVKBRendererLayoutInfo, 1);
        int i;

        li->num_layouts = priv->layout->num_sub_layouts;
        li->type = g_new0(guint, li->num_layouts);
        li->label = g_new0(gchar *, li->num_layouts);

        for (i = 0; i < li->num_layouts; i++)
        {
          li->type[i] = priv->layout->sub_layouts[i].type;
          li->label[i] = g_strdup(priv->layout->sub_layouts[i].label);
        }

        if (priv->sub_layout || (priv->sub_layout = priv->current_sub_layout))
          li->num_rows = priv->sub_layout->key_sections->num_rows;
        else
          li->num_rows = -1;

        g_value_set_pointer(value, li);
      }
      break;
    case HILDON_VKB_RENDERER_PROP_LAYOUTS:
      if (priv->layout_collection)
      {
        HildonVKBRendererLayoutInfo *li =
            g_new0(HildonVKBRendererLayoutInfo, 1);
        int i;

        li->num_layouts = priv->layout_collection->num_layouts;
        li->type = g_new0(guint, li->num_layouts);
        li->label = NULL;

        for (i = 0; i < li->num_layouts; i++)
          li->type [i] = priv->layout_collection->layout_types[i];

        if (priv->sub_layout ||
            (priv->sub_layout = priv->current_sub_layout) != 0)
        {
          li->num_rows = priv->sub_layout->key_sections->num_rows;
        }
        else
          li->num_rows = -1;

        g_value_set_pointer(value, li);
      }
      else
        g_value_set_pointer(value, NULL);

      break;
    case HILDON_VKB_RENDERER_PROP_SUBLAYOUT:
      if (!priv->sub_layout &&
          !hildon_vkb_renderer_load_layout(HILDON_VKB_RENDERER(object), TRUE))
      {
        g_value_set_int(value, 0);
      }
      else
        g_value_set_int(value, priv->num_sub_layouts);
      break;
    case HILDON_VKB_RENDERER_PROP_COLLECTION_FILE:
      if (priv->layout_collection)
        g_value_set_string(value, priv->layout_collection->filename);
      else
        g_value_set_string(value, FALSE);
      break;
    case HILDON_VKB_RENDERER_PROP_LAYOUT:
      if (priv->layout ||
          hildon_vkb_renderer_load_layout(HILDON_VKB_RENDERER(object), TRUE))
        g_value_set_int(value, priv->layout_type);
      else
        g_value_set_int(value, 0);

      break;
    case HILDON_VKB_RENDERER_PROP_DIMENSION:
      g_value_set_boxed(value, &priv->requisition);
      break;
  }
}

static void
hildon_vkb_renderer_update_option(GObject * self)
{
  HildonVKBRenderer *renderer;
  HildonVKBRendererPrivate *priv;
  vkb_sub_layout *sub_layout;
  int i,j;

  tracef;
  g_return_if_fail (HILDON_IS_VKB_RENDERER (self));

  renderer = HILDON_VKB_RENDERER(self);
  priv = HILDON_VKB_RENDERER_GET_PRIVATE(renderer);
  sub_layout = priv->sub_layout;

  if (!sub_layout)
    return;

  for (i = 0; i < sub_layout->num_key_sections; i++)
  {
    vkb_key_section *section = &sub_layout->key_sections[i];

    for (j = 0; section->num_keys; j++)
    {
      vkb_key *key = &section->keys[j];

      if (key->key_type & KEY_TYPE_HEXA)
        hildon_vkb_renderer_key_update(renderer, key, -1, TRUE);
    }
  }
}

static void
hildon_vkb_renderer_set_property(GObject *object, guint prop_id,
                                 const GValue *value, GParamSpec *pspec)
{
  HildonVKBRendererPrivate *priv;

  tracef;
  g_return_if_fail(HILDON_IS_VKB_RENDERER(object));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(HILDON_VKB_RENDERER(object));

  switch (prop_id)
  {
    case HILDON_VKB_RENDERER_PROP_GESTURE_RANGE:
    {
      priv->gesture_range = g_value_get_int(value);
      break;
    }
    case HILDON_VKB_RENDERER_PROP_CYCLE_TIMEOUT:
    {
      priv->sliding_key_timeout = g_value_get_int(value);
      break;
    }
    case HILDON_VKB_RENDERER_PROP_REPEAT_INTERVAL:
    {
      priv->key_repeat_interval = g_value_get_int(value);
      break;
    }
    case HILDON_VKB_RENDERER_PROP_STYLE_NORMAL:
    case HILDON_VKB_RENDERER_PROP_STYLE_SPECIAL:
    case HILDON_VKB_RENDERER_PROP_STYLE_SLIDE:
    case HILDON_VKB_RENDERER_PROP_STYLE_WHITESPACE:
    case HILDON_VKB_RENDERER_PROP_STYLE_TAB:
    case HILDON_VKB_RENDERER_PROP_STYLE_BACKSPACE:
    case HILDON_VKB_RENDERER_PROP_STYLE_SHIFT:
    {
      update_style(HILDON_VKB_RENDERER(object), g_value_get_string(value),
                   prop_id, FALSE);
      break;
    }
    case HILDON_VKB_RENDERER_PROP_OPTION:
    {
      if (priv->sub_layout)
      {
        if (priv->secondary_layout == g_value_get_boolean(value))
          return;

        priv->secondary_layout = g_value_get_boolean(value);
        hildon_vkb_renderer_update_option(object);
      }
      else
        priv->secondary_layout = g_value_get_boolean(value);

      break;
    }
    case HILDON_VKB_RENDERER_PROP_MODE:
    {
      if (priv->sub_layout)
      {
        if (priv->mode_bitmask != g_value_get_int(value))
        {
          priv->mode_bitmask = g_value_get_int(value);
          hildon_vkb_renderer_update_mode(HILDON_VKB_RENDERER (object), TRUE);
        }
      }
      else
        priv->mode_bitmask = g_value_get_int(value);

      break;
    }
    case HILDON_VKB_RENDERER_PROP_SUBLAYOUT:
    {
      priv->num_sub_layouts = g_value_get_int(value);

      if (priv->sub_layout)
      {
        priv->current_sub_layout = priv->sub_layout;
        priv->sub_layout_changed = TRUE;
      }

      priv->sub_layout = NULL;
      break;
    }
    case HILDON_VKB_RENDERER_PROP_DIMENSION:
    {
      GtkRequisition *requisition = g_value_get_boxed(value);

      priv->requisition.height = requisition->height;
      priv->requisition.width = requisition->width;
      break;
    }
    case HILDON_VKB_RENDERER_PROP_COLLECTION_FILE:
    {
      const gchar *collection_file = g_value_get_string(value);

      if (!collection_file)
        return;

      if (priv->layout_collection)
      {
        if (strcmp(priv->layout_collection->filename, collection_file))
          imlayout_vkb_free_layout_collection(priv->layout_collection);
        else
          break;
      }

      priv->layout_collection = imlayout_vkb_load_file(collection_file);
    }
    case HILDON_VKB_RENDERER_PROP_LAYOUT:
    {
      if (prop_id == HILDON_VKB_RENDERER_PROP_LAYOUT)
        priv->layout_type = g_value_get_int(value);

      if (priv->layout)
      {
        if (priv->WC_language)
          imlayout_vkb_free_layout(priv->WC_language);

        priv->WC_language = priv->layout;
        priv->current_sub_layout = priv->sub_layout;
      }

      priv->layout = NULL;
      priv->sub_layout = NULL;

      if (priv->dead_key)
      {
        g_signal_emit(HILDON_VKB_RENDERER(object), signals[COMBINING_INPUT],
                      0, NULL);
      }

      priv->dead_key = NULL;
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
    }
  }
}

static void
hildon_vkb_renderer_paint_pixmap(HildonVKBRenderer *self)
{
  HildonVKBRendererPrivate *priv;
  vkb_sub_layout *sub_layout;
  int i;

  tracef;
  g_return_if_fail(HILDON_IS_VKB_RENDERER(self));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);

  if (!priv->sub_layout)
  {
    priv->paint_pixmap_pending = TRUE;
    return;
  }

  if (!priv->pixmap)
    return;

  priv->paint_pixmap_pending = FALSE;
  imlayout_vkb_init_buttons(priv->layout_collection,
                            priv->layout,
                            GTK_WIDGET(self)->allocation.width,
                            GTK_WIDGET(self)->allocation.height);
  gdk_draw_rectangle(priv->pixmap,
                     GTK_WIDGET(self)->style->bg_gc[GTK_STATE_NORMAL],
                     TRUE, 0, 0,
                     priv->requisition.width,
                     priv->requisition.height);
  sub_layout = priv->sub_layout;

  for (i = 0; i < sub_layout->num_key_sections; i++)
  {
    vkb_key_section *key_section = &sub_layout->key_sections[i];
    int j;

    for (j = 0; j < key_section->num_keys; j++)
    {
      vkb_key *key = &key_section->keys[j];

      if(!priv->dead_key ||
         g_strcmp0((gchar *)priv->dead_key->labels,
                   (gchar *)key->labels))
      {
        if((key->key_flags & KEY_TYPE_SHIFT) == KEY_TYPE_SHIFT)
        {
          if (!priv->shift_active)
            key->gtk_state = GTK_STATE_NORMAL;
          else
            key->gtk_state = GTK_STATE_ACTIVE;
        }
      }
      else
      {
        key->gtk_state = GTK_STATE_ACTIVE;
        priv->dead_key->gtk_state = GTK_STATE_ACTIVE;
        priv->sub_layout_changed = FALSE;
      }

      hildon_vkb_renderer_key_update(self, key, -1, FALSE);
    }
  }
}

static void
hildon_vkb_renderer_size_allocate(GtkWidget *widget, GdkRectangle *allocation)
{
  GdkRectangle rect;

  tracef;
  memcpy(&rect, &widget->allocation, sizeof(rect));

  g_return_if_fail(HILDON_IS_VKB_RENDERER(widget));

  memcpy(&widget->allocation, allocation, sizeof(widget->allocation));

  if(GTK_WIDGET_REALIZED(GTK_OBJECT(widget)))
  {
    gdk_window_move_resize(widget->window, allocation->x, allocation->y,
                           allocation->width, allocation->height);
  }

  if (allocation->width != rect.width || allocation->height != rect.height)
    hildon_vkb_renderer_paint_pixmap(HILDON_VKB_RENDERER(widget));
}

static void
hildon_vkb_renderer_realize(GtkWidget *widget)
{
  HildonVKBRendererPrivate *priv;
  GdkScreen *gdkscreen;
  GdkDisplay *gdkdisplay;
  int root_depth;
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
  attributes.event_mask =
      gtk_widget_get_events(widget) | GDK_EXPOSURE_MASK |
      GDK_BUTTON_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK;
  attributes.visual = gtk_widget_get_visual(widget);
  attributes.colormap = gtk_widget_get_colormap(widget);
  attributes.wclass = GDK_INPUT_OUTPUT;

  widget->window = gdk_window_new(
        gtk_widget_get_parent_window(widget), &attributes,
        GDK_WA_X | GDK_WA_Y | GDK_WA_COLORMAP | GDK_WA_VISUAL);
  gdk_window_set_user_data(widget->window, widget);

  widget->style = gtk_style_attach(widget->style, widget->window);
  gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

  gdkscreen = gtk_widget_get_screen(widget);
  gdkdisplay = gdk_screen_get_display(gdkscreen);

  root_depth = DefaultDepthOfScreen(
        XScreenOfDisplay(gdk_x11_display_get_xdisplay(gdkdisplay),
                         gdk_x11_screen_get_screen_number(gdkscreen)));

  priv->pixmap = gdk_pixmap_new(NULL,
                                priv->requisition.width,
                                priv->requisition.height,
                                root_depth);

  cr = gdk_cairo_create(widget->window);
  priv->pango_layout = pango_cairo_create_layout(cr);

  if (priv->font_desc)
    pango_font_description_free(priv->font_desc);

  priv->font_desc = pango_font_description_copy(widget->style->font_desc);
  pango_font_description_set_size(
        priv->font_desc,
        1.2 * pango_font_description_get_size(priv->font_desc));

  if (priv->pango_layout)
  {
    pango_layout_set_font_description(priv->pango_layout,
                                      priv->font_desc);
  }

  cairo_destroy(cr);
}

static void
hildon_vkb_renderer_unrealize(GtkWidget *widget)
{
  HildonVKBRendererPrivate *priv;

  tracef;
  g_return_if_fail(HILDON_IS_VKB_RENDERER(widget));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(HILDON_VKB_RENDERER(widget));

  gdk_x11_display_get_xdisplay(
        gdk_screen_get_display(gtk_widget_get_screen(widget)));
  gdk_x11_visual_get_xvisual(gdk_drawable_get_visual(widget->window));
  gdk_x11_colormap_get_xcolormap(gdk_drawable_get_colormap(widget->window));

  if (priv->pixmap)
  {
    g_object_unref(priv->pixmap);
    priv->pixmap = NULL;
  }

  if (priv->pango_layout)
  {
    g_object_unref(priv->pango_layout);
    priv->pango_layout = NULL;
  }

  if (GTK_WIDGET_CLASS(hildon_vkb_renderer_parent_class)->unrealize)
    GTK_WIDGET_CLASS(hildon_vkb_renderer_parent_class)->unrealize(widget);
}

static void
hildon_vkb_renderer_finalize(GObject *obj)
{
  HildonVKBRendererPrivate *priv;

  tracef;
  g_return_if_fail(HILDON_IS_VKB_RENDERER(obj));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(HILDON_VKB_RENDERER(obj));
  gdk_x11_display_get_xdisplay(
        gdk_screen_get_display(gtk_widget_get_screen(GTK_WIDGET(obj))));

  if (priv->layout)
    imlayout_vkb_free_layout(priv->layout);

  if (priv->WC_language)
    imlayout_vkb_free_layout(priv->WC_language);

  if (priv->layout_collection)
    imlayout_vkb_free_layout_collection(priv->layout_collection);

  g_free(priv->normal.style);
  g_free(priv->special.style);
  g_free(priv->slide.style);
  g_free(priv->whitespace.style);
  g_free(priv->tab.style);
  g_free(priv->backspace.style);
  g_free(priv->shift.style);

  if (priv->key_repeat_timer)
    g_source_remove(priv->key_repeat_timer);

  if (priv->key_repeat_init_timer)
    g_source_remove(priv->key_repeat_init_timer);

  if (priv->sliding_key_timer)
    g_source_remove(priv->sliding_key_timer);

  if (G_OBJECT_CLASS(hildon_vkb_renderer_parent_class)->finalize)
    G_OBJECT_CLASS(hildon_vkb_renderer_parent_class)->finalize(obj);
}

static int
hildon_vkb_renderer_font_has_char(HildonVKBRenderer *self, gunichar uc)
{

  cairo_t* ct;
  PangoLayout * layout;
  gchar utf[6];
  gint num_chars;
  int unknown_glyphs_count;

  tracef;
  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(self),TRUE);

  ct = gdk_cairo_create(GTK_WIDGET(self)->window);
  num_chars = g_unichar_to_utf8(uc, utf);
  layout = pango_cairo_create_layout(ct);
  pango_layout_set_text(layout, utf, num_chars);
  unknown_glyphs_count = pango_layout_get_unknown_glyphs_count(layout);
  g_object_unref(layout);
  cairo_destroy(ct);

  return unknown_glyphs_count == 0;
}

static void
hildon_vkb_renderer_accent_combine_input(HildonVKBRenderer *self,
                                         const char *combine, gboolean unk)
{
  HildonVKBRendererPrivate *priv;
  gchar *norm;
  gchar *utf;
  gunichar ucs[2];

  tracef;
  g_return_if_fail(HILDON_IS_VKB_RENDERER(self));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);

  if (!priv->dead_key)
    return;

  switch (g_utf8_get_char((gchar *)priv->dead_key->labels))
  {
    case 0x22:
      ucs[1] = 0x30B;
      break;
    case 0x27:
      ucs[1] = 0x301;
      break;
    case 0x5E:
      ucs[1] = 0x302;
      break;
    case 0x60:
      ucs[1] = 0x300;
      break;
    case 0x7E:
      ucs[1] = 0x303;
      break;
    case 0xA8:
      ucs[1] = 0x308;
      break;
    case 0xAF:
      ucs[1] = 0x304;
      break;
    case 0xB0:
      ucs[1] = 0x30A;
      break;
    case 0xB4:
      ucs[1] = 0x301;
      break;
    case 0xB8:
      ucs[1] = 0x327;
      break;
    case 0x2BD:
      ucs[1] = 0x314;
      break;
    case 0x2C7:
      ucs[1] = 0x30C;
      break;
    case 0x2C8:
      ucs[1] = 0x30D;
      break;
    case 0x2D8:
      ucs[1] = 0x306;
      break;
    case 0x2D9:
      ucs[1] = 0x307;
      break;
    case 0x2DA:
      ucs[1] = 0x30A;
      break;
    case 0x2DB:
      ucs[1] = 0x328;
      break;
    case 0x2DD:
      ucs[1] = 0x30B;
      break;
    case 0x385:
      ucs[1] = 0x344;
      break;
    default:
      ucs[1] = 0;
      break;
  }

  ucs[0] = g_utf8_get_char(combine);
  utf = g_ucs4_to_utf8(ucs, G_N_ELEMENTS(ucs), NULL, NULL, NULL);
  norm = g_utf8_normalize(utf, -1, G_NORMALIZE_DEFAULT_COMPOSE);

  if (strlen(utf) > strlen(norm) && strcmp(utf, norm) &&
      hildon_vkb_renderer_font_has_char(HILDON_VKB_RENDERER(self),
                                        g_utf8_get_char(norm)))
  {
    g_signal_emit(self, signals[INPUT], 0, norm, unk);
  }
  else
    g_signal_emit(self, signals[INPUT], 0, combine, unk);

  g_free(utf);
  g_free(norm);
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

  if (!sliding_key)
    return;

  if (unk)
  {
    if (sliding_key->current_slide_key)
      label = sliding_key->labels[sliding_key->current_slide_key - 1];
    else
      label = sliding_key->labels[sliding_key->byte_count - 1];

    sliding_key->width = 0;
    priv->sliding_key->current_slide_key = 0;

    gtk_widget_queue_draw_area(
          GTK_WIDGET(self),
          priv->sliding_key->left,
          priv->sliding_key->top,
          priv->sliding_key->right - priv->sliding_key->left,
          priv->sliding_key->bottom - priv->sliding_key->top);

    priv->sliding_key = 0;

    if (priv->sliding_key_timer)
    {
      g_source_remove(priv->sliding_key_timer);
      priv->sliding_key_timer = 0;
    }
  }
  else
    label = sliding_key->labels[sliding_key->current_slide_key];

  if (priv->dead_key)
    hildon_vkb_renderer_accent_combine_input(self, label, unk);
  else
    g_signal_emit(self, signals[INPUT], 0, label, unk);
}

static gboolean
is_shift_or_dead_key(vkb_key *key)
{
  if (key->key_flags & KEY_TYPE_DEAD)
    return TRUE;

  return ((key->key_flags & KEY_TYPE_SHIFT) == KEY_TYPE_SHIFT);
}

static gboolean
hildon_vkb_renderer_button_release(GtkWidget *widget, GdkEventButton *event)
{
  HildonVKBRenderer *self;
  HildonVKBRendererPrivate *priv;
  gboolean in_range = FALSE;
  vkb_key *pressed_key;
  int gesture_range;
  vkb_key *dead_key;
  unsigned short key_flags;
  int shift;

  tracef;
  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(widget),TRUE);

  self = HILDON_VKB_RENDERER(widget);
  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);
  pressed_key = priv->pressed_key;

  if (event->button != 1 || !pressed_key || !priv->sub_layout)
    goto out;

  priv->max_distance_exceeded = FALSE;
  gesture_range = priv->gesture_range;

  if (event->x > ((int)pressed_key->left - gesture_range) &&
      event->x <= (pressed_key->right + gesture_range) &&
      event->y > ((int)pressed_key->top - gesture_range) &&
      event->y <= (pressed_key->bottom + gesture_range) &&
      !priv->key_repeat_timer)
  {
    in_range = TRUE;

    if (!is_shift_or_dead_key(pressed_key))
    {
      if (priv->sliding_key_timeout &&
          (pressed_key->key_type & KEY_TYPE_SLIDING))
      {
        priv->sliding_key = pressed_key;
        hildon_vkb_renderer_input_slide(self, FALSE);

        if (pressed_key->current_slide_key >= pressed_key->byte_count - 1)
          pressed_key->current_slide_key = 0;
        else
          pressed_key->current_slide_key++;

        pressed_key->width = 0;
        priv->sliding_key_timer =
            g_timeout_add(priv->sliding_key_timeout,
                          hildon_vkb_renderer_sliding_key_timeout,
                          widget);
      }
      else
        hildon_vkb_renderer_input_key(self);
    }
  }

  if (priv->key_repeat_timer)
  {
    g_source_remove(priv->key_repeat_timer);
    priv->key_repeat_timer = 0;
  }

  if (pressed_key)
  {
    dead_key = priv->dead_key;
    key_flags = pressed_key->key_flags;
    shift = pressed_key->key_flags & KEY_TYPE_SHIFT;

    if (shift || !dead_key || dead_key == pressed_key)
    {
      if ((shift == KEY_TYPE_SHIFT) && in_range)
      {
        GtkStateType state;

        priv->shift_active = !priv->shift_active;

        if (priv->shift_active)
          state = GTK_STATE_ACTIVE;
        else
          state = GTK_STATE_NORMAL;

        hildon_vkb_renderer_key_update(self, pressed_key, state, TRUE);
        hildon_vkb_renderer_input_key(self);
      }
      else if (key_flags & KEY_TYPE_DEAD && in_range)
      {
        if (priv->dead_key == pressed_key)
        {
          hildon_vkb_renderer_key_update(
                self, pressed_key, GTK_STATE_NORMAL, TRUE);

          if (priv->dead_key)
          {
            g_signal_emit(self, signals[COMBINING_INPUT], 0, NULL);
            hildon_vkb_renderer_input_key(self);
          }

          priv->dead_key = NULL;
        }
        else
        {
          hildon_vkb_renderer_key_update(
                self, pressed_key, GTK_STATE_ACTIVE, TRUE);

          priv->dead_key = pressed_key;
          g_signal_emit(self, signals[COMBINING_INPUT], 0, pressed_key->labels);
        }
      }
      else if (!is_shift_or_dead_key(pressed_key))
      {
        hildon_vkb_renderer_key_update(
              self, pressed_key, GTK_STATE_NORMAL, TRUE);
        g_signal_emit(self, signals[TEMP_INPUT], 0, NULL, TRUE);
      }
    }
    else
    {
      if (!priv->sub_layout_changed)
        hildon_vkb_renderer_key_update(self, dead_key, GTK_STATE_NORMAL, TRUE);

      priv->dead_key = 0;
    }
  }

out:
  hildon_vkb_renderer_release_cleanup(priv);

  return TRUE;
}

static void
get_key_from_coordinates(HildonVKBRenderer *self, GdkEventMotion *event)
{
  HildonVKBRendererPrivate *priv;
  vkb_key *key;
  int i, j;

  tracef;
  g_return_if_fail(self);

  priv=HILDON_VKB_RENDERER_GET_PRIVATE(self);

  if (priv->sub_layout)
  {
    for (i = 0; i < priv->sub_layout->num_key_sections; i++)
    {
      for (j = 0; j < priv->sub_layout->key_sections[i].num_keys; j++)
      {
        key = &priv->sub_layout->key_sections[i].keys[j];

        if (event->x > key->left && event->x <= key->right &&
            event->y > key->top && event->y <= key->bottom)
        {
          if (key->gtk_state != GTK_STATE_INSENSITIVE &&
              key != priv->pressed_key && !priv->key_repeat_timer)
          {
            hildon_vkb_renderer_key_update(HILDON_VKB_RENDERER(self),
                                           priv->pressed_key,
                                           GTK_STATE_NORMAL, TRUE);
            priv->pressed_key = key;
            hildon_vkb_renderer_key_update(HILDON_VKB_RENDERER(self), key,
                                           GTK_STATE_ACTIVE, TRUE);

            if (!(priv->pressed_key->key_type & KEY_TYPE_SLIDING))
            {
              g_signal_emit(
                    HILDON_VKB_RENDERER(self), signals[TEMP_INPUT], 0,
                    priv->pressed_key->labels, TRUE);
            }
          }

          return;
        }
      }
    }
  }

  if (priv->pressed_key)
  {
    if (priv->pressed_key->gtk_state != GTK_STATE_INSENSITIVE)
    {
      hildon_vkb_renderer_key_update(HILDON_VKB_RENDERER(self),
                                     priv->pressed_key, GTK_STATE_NORMAL, TRUE);

      if (!(priv->pressed_key->key_type & KEY_TYPE_SLIDING))
      {
        g_signal_emit(
              HILDON_VKB_RENDERER(self), signals[TEMP_INPUT], 0, NULL, TRUE);
      }
    }
  }
}

static gboolean
hildon_vkb_renderer_motion_notify(GtkWidget *widget, GdkEventMotion *event)
{
  HildonVKBRendererPrivate *priv;
  int diff_x;
  int diff_y;

  tracef;
  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(widget), TRUE);

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(HILDON_VKB_RENDERER(widget));

  if (!priv->pressed_key)
    return 0;

  if(priv->pressed_key->left >= event->x ||
     event->x >= priv->pressed_key->right ||
     priv->pressed_key->top >= event->y ||
     event->y >= priv->pressed_key->bottom)
  {
    if (priv->key_repeat_init_timer)
    {
      g_source_remove(priv->key_repeat_init_timer);
      priv->key_repeat_init_timer = 0;
    }

    if (priv->key_repeat_timer)
    {
      g_source_remove(priv->key_repeat_timer);
      priv->key_repeat_timer = 0;
      priv->track_position = FALSE;
    }

    priv->sliding_key = 0;

    diff_x = (event->x - priv->x);

    if (diff_x < 0)
      diff_x = -diff_x;

    diff_y = (event->y - priv->y);

    if (diff_y < 0)
      diff_y = -diff_y;

    if(diff_x > 50 || diff_y > 50)
    {
      if (!priv->max_distance_exceeded)
      {
        priv->max_distance_exceeded = TRUE;
        hildon_vkb_renderer_input_key(HILDON_VKB_RENDERER(widget));
      }

      priv->track_position = FALSE;
    }
  }

  get_key_from_coordinates(HILDON_VKB_RENDERER(widget), event);

  if (priv->track_position)
  {
    priv->x = event->x;
    priv->y = event->y;
  }

  return TRUE;
}

static gboolean
hildon_vkb_renderer_expose(GtkWidget *widget, GdkEventExpose *event)
{
  HildonVKBRenderer *self;
  HildonVKBRendererPrivate *priv;
  vkb_sub_layout *sub_layout;
  GtkStyle *style;
  PangoLayoutIter *iter;
  cairo_t *ct;
  gchar *text;
  int height;
  int i;
  int n_rectangles;
  GdkRectangle *rectangles;

  tracef;
  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(widget),FALSE);

  self = HILDON_VKB_RENDERER(widget);
  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);

  if (!((priv->sub_layout || hildon_vkb_renderer_load_layout(self, TRUE) ||
         priv->sub_layout) && priv->pixmap))
  {
    return FALSE;
  }

  if (priv->paint_pixmap_pending)
    hildon_vkb_renderer_paint_pixmap(self);

  gdk_region_get_rectangles(event->region, &rectangles, &n_rectangles);

  for (i = 0; i < n_rectangles; i++)
  {
    GdkRectangle *r = &rectangles[i];

    gdk_draw_drawable(widget->window,
                      widget->style->bg_gc[GTK_STATE_NORMAL], priv->pixmap,
                      r->x, r->y, r->x, r->y, r->width, r->height);
  }

  g_free(rectangles);

  ct = gdk_cairo_create(widget->window);
  cairo_rectangle(ct, event->area.x, event->area.y, event->area.width,
                  event->area.height);
  cairo_clip(ct);
  pango_layout_set_text(priv->pango_layout, "kq", 1);
  pango_layout_get_pixel_size(priv->pango_layout, &i, &height);

  sub_layout = priv->sub_layout;


  for (i = 0; i < sub_layout->num_key_sections; i++)
  {
    vkb_key_section *key_section = &sub_layout->key_sections[i];
    int j;

    for (j = 0; j < key_section->num_keys; j++)
    {
      vkb_key *key = &key_section->keys[j];
      GdkRectangle src;
      GdkRectangle dest;
      gchar *tmp_text = NULL;

      if (priv->secondary_layout && key->num_sub_keys)
        key = key->sub_keys;

      src.x = key->left;
      src.y = key->top;
      src.width = key->right - key->left;
      src.height = key->bottom - key->top;

      if (!gdk_rectangle_intersect(&event->area, &src, &dest))
        continue;

      if (key->key_flags & KEY_TYPE_DEAD)
        style = priv->special.gtk_style;
      else
        style = priv->normal.gtk_style;

      gdk_cairo_set_source_color(ct, &style->fg[key->gtk_state]);

      if(key->key_type & KEY_TYPE_SLIDING)
      {
        gchar **labels = key->labels;
        unsigned char byte_count = key->byte_count;

        if (byte_count > 2)
        {
          int next = 2, current = 1, prev = 0;

          if (key->current_slide_key)
          {
            next = key->current_slide_key + 1;
            current = key->current_slide_key;
            prev = key->current_slide_key - 1;
          }
          else if (priv->sliding_key_timer)
          {
            next = byte_count + 1;
            current = byte_count;
            prev = byte_count - 1;
          }

          tmp_text = g_strconcat(labels[prev % byte_count], " ",
                                 labels[current % byte_count], " ",
                                 labels[next % byte_count],
                                 NULL);
          text = tmp_text;
        }
        else
          text = labels[key->current_slide_key];
      }
      else if ((key->key_type & KEY_TYPE_MULTIPLE) && key->sub_keys &&
               key->num_sub_keys > 1)
      {
        if (priv->secondary_layout)
          text = (gchar *)key->sub_keys[1].labels;
        else
          text = (gchar *)key->sub_keys->labels;
      }
      else
        text = (gchar *)key->labels;

      pango_layout_set_text(priv->pango_layout, text, strlen(text));
      pango_layout_get_pixel_size(priv->pango_layout, (int *)&key->width, NULL);

      iter = pango_layout_get_iter(priv->pango_layout);
      key->offset = height / 2 + (key->bottom - key->top) / 2;
      pango_layout_iter_free(iter);

      if ((key->key_type & KEY_TYPE_SLIDING) && key->byte_count > 2)
      {
        cairo_move_to(ct, key->left + (key->right - key->left - key->width) / 2,
                      key->top + (key->bottom - key->top - key->offset) / 2);

        cairo_save(ct);
        gdk_cairo_set_source_color(
              ct, &priv->slide.gtk_style->fg[GTK_STATE_INSENSITIVE]);
        pango_cairo_show_layout(ct, priv->pango_layout);
        cairo_restore(ct);

        if (priv->sliding_key_timer)
        {
          pango_layout_set_text(priv->pango_layout, text,
                                g_utf8_next_char(text) - text);
          pango_cairo_show_layout(ct, priv->pango_layout);
        }
      }
      else
      {
        cairo_move_to(ct,
                      (key->left + ((key->right - key->left - key->width) / 2)),
                      (key->top + (key->bottom - key->top - key->offset) / 2));
        pango_cairo_show_layout(ct, priv->pango_layout);
      }

      g_free(tmp_text);
    }
  }

  cairo_destroy(ct);

  return TRUE;
}

static gboolean
hildon_vkb_renderer_button_press(GtkWidget *widget, GdkEventButton *event)
{
  HildonVKBRenderer *renderer;
  HildonVKBRendererPrivate *priv;
  int i, j;

  tracef;
  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(widget), TRUE);

  renderer = HILDON_VKB_RENDERER(widget);
  priv = HILDON_VKB_RENDERER_GET_PRIVATE(renderer);

  if (event->button != 1 || !priv->sub_layout)
    return TRUE;

  if (priv->key_repeat_init_timer)
  {
    g_source_remove(priv->key_repeat_init_timer);
    priv->key_repeat_init_timer = 0;
  }

  if (priv->key_repeat_timer)
  {
    g_source_remove(priv->key_repeat_timer);
    priv->key_repeat_timer = 0;
  }

  if (priv->sliding_key_timer)
  {
    g_source_remove(priv->sliding_key_timer);
    priv->sliding_key_timer = 0;
  }

  for (i = 0; i < priv->sub_layout->num_key_sections; i++)
  {
    vkb_key_section *key_section = &priv->sub_layout->key_sections[i];

    for (j = 0; j < key_section->num_keys; j++)
    {
      vkb_key *key = &key_section->keys[j];

      if (event->x > key->left && event->x <= key->right &&
          event->y > key->top && event->y <= key->bottom)
      {
        if (key->gtk_state == GTK_STATE_INSENSITIVE)
        {
          gpointer label;

          if (key->key_type & KEY_TYPE_SLIDING)
            label = *key->labels;
          else
            label = key->labels;

          g_signal_emit(renderer, signals[ILLEGAL_INPUT], 0, label);
          return TRUE;
        }

        priv->pressed_key = key;

        if (!(key->key_type & KEY_TYPE_SLIDING) &&
            !(key->key_flags & KEY_TYPE_SHIFT))
        {
          g_signal_emit(renderer, signals[TEMP_INPUT], 0,
                        priv->pressed_key->labels, TRUE);
        }

        if (priv->sliding_key && priv->sliding_key != key)
          hildon_vkb_renderer_input_slide(renderer, TRUE);

        if (!is_shift_or_dead_key(key))
        {
          hildon_vkb_renderer_key_update(
                renderer, priv->pressed_key, GTK_STATE_ACTIVE, TRUE);
        }

        if (key->key_type == KEY_TYPE_NORMAL)
        {
          if (priv->key_repeat_interval)
          {
            if (!(key->key_flags & 0x200) && !is_shift_or_dead_key(key))
            {
              priv->key_repeat_init_timer  =
                  g_timeout_add(priv->key_repeat_interval + 625,
                                hildon_vkb_renderer_key_repeat_init, widget);
            }

            priv->x = event->x;
            priv->y = event->y;
            break;
          }
        }

        priv->track_position = FALSE;
      }
    }
  }

  return TRUE;
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

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_DIMENSION,
        g_param_spec_boxed(
          "dimension",
          "max dimension of the pixmap",
          "GtkReqisition defining the size of the pixmap",
          GTK_TYPE_REQUISITION,
          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)
        );

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_COLLECTION_FILE,
        g_param_spec_string(
          "collection",
          "collection file name",
          "the file name of the layout collection to be used",
          NULL,
          G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_LAYOUT,
        g_param_spec_int(
          "layout",
          "current active layout",
          "currently active layout indicated by layout type",
          0,
          8,
          0,
          G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_SUBLAYOUT,
        g_param_spec_int(
          "sub",
          "current active sublayout",
          "currently active sublayout indicated by sublayout index",
          0,
          G_MAXINT,
          0,
          G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_LAYOUTS,
        g_param_spec_pointer(
          "layouts",
          "layouts inside a collection",
          "tells all the layouts this current collection have",
          G_PARAM_READABLE));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_SUBLAYOUTS,
        g_param_spec_pointer(
          "subs",
          "sublayout inside a layout",
          "tells all the sublayout this widget can activate",
          G_PARAM_READABLE));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_MODE,
        g_param_spec_int(
          "mode",
          "mode bitmask",
          "enable/disable mode for this layout",
          KEY_TYPE_ALPHA,
          KEY_TYPE_ALL,
          KEY_TYPE_ALL,
          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_OPTION,
        g_param_spec_boolean(
          "option",
          "secondary option layout",
          "turn on/off secondary layout with secondary layout",
          0,
          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_STYLE_NORMAL,
        g_param_spec_string(
          "style_normal",
          "normal style path",
          "string defines widget path of the style for normal keys",
          ".osso-im-key",
          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_STYLE_SPECIAL,
        g_param_spec_string(
          "style_special",
          "special style path",
          "string defines widget path of the style for special keys",
          ".osso-im-accent",
          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_STYLE_SLIDE,
        g_param_spec_string(
          "style_slide",
          "slide style path",
          "string defines widget path of the style for sliding keys",
          ".osso-im-key",
          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_STYLE_WHITESPACE,
        g_param_spec_string(
          "style_whitespace",
          "whitespace style path",
          "string defines widget path of the style for whitespace keys",
          ".osso-im-key",
          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_STYLE_TAB,
        g_param_spec_string(
          "style_tab",
          "Tab style path",
          "string defines widget path of the style for TAB key",
          ".osso-im-key",
          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_STYLE_BACKSPACE,
        g_param_spec_string(
          "style_backspace",
          "backspace style path",
          "string defines widget path of the style for backspace keys",
          ".osso-im-key",
          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_STYLE_SHIFT,
        g_param_spec_string(
          "style_shift",
          "shift style path",
          "string defines widget path of the style for shift keys",
          ".osso-im-key",
          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_REPEAT_INTERVAL,
        g_param_spec_int(
          "repeat_interval",
          "repeat rate in ms",
          "set length of the key repeating rate",
          0,
          G_MAXINT,
          0,
          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_CYCLE_TIMEOUT,
        g_param_spec_int(
          "cycle_timeout",
          "the timeout for sliding key in ms",
          "set the timeout for finalized slidingkey input",
          0,
          G_MAXINT,
          750,
          G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_WC_LANGUAGE,
        g_param_spec_string(
          "WC_language",
          "word completion language",
          "string tells the defined word completion language for current collection",
          0,
          G_PARAM_READABLE));

  g_object_class_install_property(
        object_class, HILDON_VKB_RENDERER_PROP_GESTURE_RANGE,
        g_param_spec_int(
          "gesture_range",
          "the gesture recognition range threshold in px",
          "set the range for gestures",
          0,
          G_MAXINT,
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
hildon_vkb_renderer_key_update(HildonVKBRenderer *self, vkb_key *key,
                               gint state, gboolean repaint)
{
  HildonVKBRendererPrivate *priv;
  GtkStyle *style;
  vkb_key *tmp_key;
  tracef;

  g_return_if_fail(HILDON_IS_VKB_RENDERER(self));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(self);

  if((key->key_type & KEY_TYPE_HEXA) && (key->sub_keys) &&
     (key->num_sub_keys > 1))
  {
    if (priv->secondary_layout)
      tmp_key = key->sub_keys + 1;
    else
      tmp_key = key->sub_keys;
  }
  else
    tmp_key = key;

  if(state >= 0)
    tmp_key->gtk_state = state;

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
    gtk_paint_box(style, priv->pixmap, tmp_key->gtk_state, 1, 0,
                  GTK_WIDGET(self), 0, key->left, key->top,
                  key->right - key->left, key->bottom - key->top);

    if (repaint)
    {
      gtk_widget_queue_draw_area(
            GTK_WIDGET(self), key->left, key->top, key->right - key->left,
            key->bottom - key->top);
    }
  }
}

void
hildon_vkb_renderer_set_sublayout_type(HildonVKBRenderer *renderer,
                                       unsigned int sub_layout_type)
{
  HildonVKBRendererPrivate *priv;
  vkb_sub_layout *sub_layout;
  int i = 0;

  tracef;
  g_return_if_fail(HILDON_IS_VKB_RENDERER(renderer));

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(renderer);

  if (priv->layout)
  {
    if (priv->sub_layout && priv->sub_layout->type == sub_layout_type)
      return;
  }
  else
  {
    if (!hildon_vkb_renderer_load_layout(renderer, FALSE))
      return;
  }

  if (priv->layout)
  {
    if (priv->layout->sub_layouts->type == sub_layout_type)
      g_object_set(renderer, "sub", 0, NULL);
    else
    {
      sub_layout = priv->layout->sub_layouts;

      while (1)
      {
        i++;

        if (i == priv->layout->num_sub_layouts)
          break;

        sub_layout++;

        if (sub_layout->type == sub_layout_type)
        {
          g_object_set(renderer, "sub", i, NULL);
          break;
        }
      }
    }
  }

  if (!priv->sub_layout)
    hildon_vkb_renderer_load_layout(renderer, TRUE);

  if (priv->layout->num_sub_layouts != i)
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
    if (priv->sub_layout_changed)
      priv->sub_layout_changed = FALSE;
    else
      hildon_vkb_renderer_key_update(renderer, priv->dead_key, 0, 1);

    priv->dead_key = 0;
    g_signal_emit(renderer, signals[COMBINING_INPUT], 0, NULL);
  }
}

gchar*
hildon_vkb_renderer_get_dead_key(HildonVKBRenderer *renderer)
{
  HildonVKBRendererPrivate *priv;

  tracef;
  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(renderer),NULL);

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(renderer);

  if (priv->dead_key)
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

  if (key)
  {
    if(priv->dead_key)
      hildon_vkb_renderer_accent_combine_input(self, (gchar *)key->labels, 1);
    else
      g_signal_emit(self, signals[INPUT], 0, key->labels, TRUE);
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
  g_return_val_if_fail(HILDON_IS_VKB_RENDERER(data), FALSE);

  priv = HILDON_VKB_RENDERER_GET_PRIVATE(HILDON_VKB_RENDERER(data));

  if (priv->pressed_key)
  {
    priv->key_repeat_timer = g_timeout_add(
                               priv->key_repeat_interval,
                               hildon_vkb_renderer_key_repeat,
                               data);

    hildon_vkb_renderer_input_key(HILDON_VKB_RENDERER(data));
  }

  priv->key_repeat_init_timer = 0;

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
