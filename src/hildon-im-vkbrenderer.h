#ifndef __HILDON_VKB_RENDERER_H_INCLDED__
#define __HILDON_VKB_RENDERER_H_INCLDED__

#define HILDON_VKB_RENDERER_TYPE (hildon_vkb_renderer_get_type())

#define HILDON_VKB_RENDERER(obj) GTK_CHECK_CAST(obj, hildon_vkb_renderer_get_type(), HildonVKBRenderer)
#define HILDON_VKB_RENDERER_CLASS(klass) \
        GTK_CHECK_CLASS_CAST(klass, hildon_vkb_renderer_get_type, \
                             HildonVKBRendererClass)
#define HILDON_IS_VKB_RENDERER(obj) \
        GTK_CHECK_TYPE(obj, HILDON_VKB_RENDERER_TYPE )
#define HILDON_VKB_RENDERER_GET_PRIVATE(obj) \
        (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HILDON_VKB_RENDERER_TYPE,\
                                      HildonVKBRendererPrivate))

typedef struct _HildonVKBRendererPrivate HildonVKBRendererPrivate;

typedef struct {
  GtkWidget parent;
  HildonVKBRendererPrivate * priv;
}HildonVKBRenderer;

typedef struct{
  guint num_layouts;
  guint *type;
  gchar **label;
  guint num_rows;
}HildonVKBRendererLayoutInfo;

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
