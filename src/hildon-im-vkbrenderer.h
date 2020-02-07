/**
   @file hildon-im-vkbrenderer3.h

   Copyright (C) 2012 Ivaylo Dimitrov <freemangordon@abv.bg>

   This file is part of libhildon-im-vkbrenderer3.

   hildon-im-vkbrenderer3 is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License
   version 2.1 as published by the Free Software Foundation.

   hildon-im-vkbrenderer3.h is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with libhildon-im-vkbrenderer3. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __HILDON_VKB_RENDERER_H_INCLDED__
#define __HILDON_VKB_RENDERER_H_INCLDED__

#define HILDON_VKB_RENDERER_TYPE (hildon_vkb_renderer_get_type())

#define HILDON_VKB_RENDERER(obj) \
  GTK_CHECK_CAST(obj, hildon_vkb_renderer_get_type(), HildonVKBRenderer)
#define HILDON_VKB_RENDERER_CLASS(klass) \
        GTK_CHECK_CLASS_CAST(klass, hildon_vkb_renderer_get_type, \
                             HildonVKBRendererClass)
#define HILDON_IS_VKB_RENDERER(obj) \
        GTK_CHECK_TYPE(obj, HILDON_VKB_RENDERER_TYPE )

typedef struct _HildonVKBRenderer HildonVKBRenderer;
typedef struct _HildonVKBRendererClass HildonVKBRendererClass;
typedef struct _HildonVKBRendererPrivate HildonVKBRendererPrivate;
typedef struct _HildonVKBRendererLayoutInfo HildonVKBRendererLayoutInfo;

struct _HildonVKBRenderer
{
  GtkWidget parent;
  HildonVKBRendererPrivate * priv;
};

struct _HildonVKBRendererClass
{
  GtkWidgetClass parent;
  void (*input) (HildonVKBRenderer*, gchar*, gboolean);
  void (*illegal_input) (HildonVKBRenderer*, gchar*);
};

/* exports */
GType hildon_vkb_renderer_get_type (void);

void hildon_vkb_renderer_clear_dead_key(HildonVKBRenderer *renderer);
gchar* hildon_vkb_renderer_get_dead_key(HildonVKBRenderer *renderer);
guint hildon_vkb_renderer_get_numeric_sub(HildonVKBRenderer *renderer);
guint hildon_vkb_renderer_get_pressed_key_mode(HildonVKBRenderer * renderer);
gboolean hildon_vkb_renderer_get_shift_active(HildonVKBRenderer * renderer);
void hildon_vkb_renderer_set_shift_active(HildonVKBRenderer *renderer, gboolean value);
void hildon_vkb_renderer_set_sublayout_type(HildonVKBRenderer *renderer, unsigned int sub_layout_type);
void hildon_vkb_renderer_set_variance_layout(HildonVKBRenderer *renderer);
void layout_info_free(HildonVKBRendererLayoutInfo *layout_info);

#endif /* __HILDON_VKB_RENDERER_H_INCLDED__ */
