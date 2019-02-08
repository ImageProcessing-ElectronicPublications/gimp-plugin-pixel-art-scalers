
//#include "config.h"
//#include <string.h>

#include <stdio.h>
#include <string.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "filter_dialog.h"

#include <stdint.h>

// Filter includes
#include "hqx.h"
#include "xbr_filters.h"
#include "filter_scalex.h"


#define BYTE_SIZE_RGBA_4BPP 4
#define BYTE_SIZE_RGB_3BPP  3

#define ALPHA_MASK_OPAQUE   0xFF  // When adding alpha mask byte, set to 100% opaque / visible

#define SCALER_STR_MAX      30

extern const char PLUG_IN_PROCEDURE[];
extern const char PLUG_IN_ROLE[];
extern const char PLUG_IN_BINARY[];

static void filter_apply (int, uint32_t *, uint32_t *, int, int);
static void resize_image_and_apply_changes(GimpDrawable *, guchar *, guint);
static void on_combo_scaler_mode_changed (GtkComboBox *, gpointer);

static void buffer_add_alpha_byte(guchar *, glong);
static void buffer_remove_alpha_byte(guchar *, glong);

enum scaler_list {
    SCALER_ENUM_FIRST = 0,

    SCALER_2X_HQX = SCALER_ENUM_FIRST,
    SCALER_2X_XBR,
    SCALER_2X_SCALEX,

    SCALER_3X_HQX,
    SCALER_3X_XBR,
    SCALER_3X_SCALEX,

    SCALER_4X_HQX,
    SCALER_4X_XBR,
    SCALER_4X_SCALEX,

    SCALER_ENUM_LAST
};



typedef struct {
    void (*scaler_function)(uint32_t*, uint32_t*, int, int);
    int  scale_factor;
    char scaler_name[SCALER_STR_MAX];
} scaler_info;

static scaler_info scalers[SCALER_ENUM_LAST];

static gint scaler_mode;

static void scalers_init(void) {

    // Init HQX scaler library
    hqxInit();
    xbr_init_data();

    // HQX
    scalers[SCALER_2X_HQX].scaler_function = &hq2x_32;
    scalers[SCALER_2X_HQX].scale_factor    = 2;
    snprintf(scalers[SCALER_2X_HQX].scaler_name, SCALER_STR_MAX, "2x HQx");

    scalers[SCALER_3X_HQX].scaler_function = &hq3x_32;
    scalers[SCALER_3X_HQX].scale_factor    = 3;
    snprintf(scalers[SCALER_3X_HQX].scaler_name, SCALER_STR_MAX, "3x HQx");

    scalers[SCALER_4X_HQX].scaler_function = &hq4x_32;
    scalers[SCALER_4X_HQX].scale_factor    = 4;
    snprintf(scalers[SCALER_4X_HQX].scaler_name, SCALER_STR_MAX, "4x HQx");


    // XBR
    scalers[SCALER_2X_XBR].scaler_function = &xbr_filter_xbr2x;
    scalers[SCALER_2X_XBR].scale_factor    = 2;
    snprintf(scalers[SCALER_2X_XBR].scaler_name, SCALER_STR_MAX, "2x XBR");

    scalers[SCALER_3X_XBR].scaler_function = &xbr_filter_xbr3x;
    scalers[SCALER_3X_XBR].scale_factor    = 3;
    snprintf(scalers[SCALER_3X_XBR].scaler_name, SCALER_STR_MAX, "3x XBR");

    scalers[SCALER_4X_XBR].scaler_function = &xbr_filter_xbr4x;
    scalers[SCALER_4X_XBR].scale_factor    = 4;
    snprintf(scalers[SCALER_4X_XBR].scaler_name, SCALER_STR_MAX, "4x XBR");


    // SCALEX
    scalers[SCALER_2X_SCALEX].scaler_function = &filter_scalex_2x;
    scalers[SCALER_2X_SCALEX].scale_factor    = 2;
    snprintf(scalers[SCALER_2X_SCALEX].scaler_name, SCALER_STR_MAX, "2x ScaleX");

    scalers[SCALER_3X_SCALEX].scaler_function = &filter_scalex_3x;
    scalers[SCALER_3X_SCALEX].scale_factor    = 3;
    snprintf(scalers[SCALER_3X_SCALEX].scaler_name, SCALER_STR_MAX, "3x ScaleX");

    scalers[SCALER_4X_SCALEX].scaler_function = &filter_scalex_4x;
    scalers[SCALER_4X_SCALEX].scale_factor    = 4;
    snprintf(scalers[SCALER_4X_SCALEX].scaler_name, SCALER_STR_MAX, "4x ScaleX");


    // Now set the default scaler
    // TODO: accept last values for plugin so it remembers
    scaler_mode = SCALER_2X_HQX;
 }


// TODO: there are probably better ways to do this than a global var
 static   GtkWidget *preview_scaled;


/*******************************************************/
/*                    Dialog                           */
/*******************************************************/


// TODO: remove globals
  GtkWidget * g_preview;
  GimpDrawable * g_drawable;


// GimpPreviewArea inherits GtkWidget::size-allocate from GtkDrawingArea
// Preview-area resets buffer on size change so it needs a redraw
gboolean preview_scaled_size_allocate_event(GtkWidget *widget, GdkEvent *event, GtkWidget *window)
{
    if (widget == NULL)
      return 1; // Exit, failed

    // TODO: Change this to just re-paint the cached size & image
    // IF (! =NULL)
    pixel_art_scalers_run (g_drawable, (GimpPreview *) g_preview);

    return FALSE;
}





gboolean pixel_art_scalers_dialog (GimpDrawable *drawable)
{
  GtkWidget *dialog;
  GtkWidget *main_vbox;
  GtkWidget *preview_hbox;
  GtkWidget *preview;
  GtkWidget * scaled_preview_window;
  GtkWidget *combo_scaler_mode;
  gboolean   run;
  gint       idx;


    // Initialize the scalers
    scalers_init();

  gimp_ui_init (PLUG_IN_BINARY, FALSE);

  dialog = gimp_dialog_new ("Pixel Art Scalers", PLUG_IN_ROLE,
                            NULL, 0,
                            gimp_standard_help_func, PLUG_IN_PROCEDURE,

                            GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                            GTK_STOCK_OK,     GTK_RESPONSE_OK,

                            NULL);

// Resize to show more of scaled preview by default (this sets MIN size)
gtk_widget_set_size_request (dialog,
                             500,
                             400);


  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           GTK_RESPONSE_OK,
                                           GTK_RESPONSE_CANCEL,
                                           -1);

  gimp_window_set_transient (GTK_WINDOW (dialog));


  // Create a main vertical box for the preview
  main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 6);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                      main_vbox, TRUE, TRUE, 0);
  gtk_widget_show (main_vbox);


  // Create a side-by-side sub-box for the pair of preview windows
  preview_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (preview_hbox), 6);
  gtk_box_pack_start (GTK_BOX (main_vbox),
                      preview_hbox, TRUE, TRUE, 0);
  gtk_widget_show (preview_hbox);


    // Create source image preview and scaled preview areas
    // along with a scrolled window area for the scaled preview
    preview = gimp_drawable_preview_new (drawable, NULL);
    preview_scaled = gimp_preview_area_new();
    scaled_preview_window = gtk_scrolled_window_new (NULL, NULL);


    // Display the source image preview area, set it to not expand if window grows
    gtk_box_pack_start (GTK_BOX (preview_hbox), preview, FALSE, TRUE, 0);
    gtk_widget_show (preview);


    // Automatic scrollbars for scrolled preview window
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scaled_preview_window),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);

    // Add the scaled preview to the scrolled window
    // and then display them both (with auto-resize)
    gtk_scrolled_window_add_with_viewport((GtkScrolledWindow *)scaled_preview_window,
                                              preview_scaled);
    gtk_box_pack_start (GTK_BOX (preview_hbox), scaled_preview_window, TRUE, TRUE, 0);
    gtk_widget_show (scaled_preview_window);
    gtk_widget_show (preview_scaled);


    // Wire up source image preview redraw to call the pixel scaler filter
    g_signal_connect_swapped (preview,
                              "invalidated",
                              G_CALLBACK (pixel_art_scalers_run),
                              drawable);

    // resize scaled preview -> destroys scaled preview buffer -> resizes scroll window -> size-allocate -> redraw preview buffer
    // TODO: remove global calls here
    // Wire up the scaled preview to redraw when ever it's size changes
    // This fixes the scrolled window inhibiting the redraw when the size changed
    g_preview = preview;
    g_drawable = drawable;
    g_signal_connect(preview_scaled, "size-allocate", G_CALLBACK(preview_scaled_size_allocate_event), (gpointer)scaled_preview_window);



    // Add a Combo box for the SCALER MODE
    // then add entries for the scaler types
    combo_scaler_mode = gtk_combo_box_text_new ();

    for (idx = SCALER_ENUM_FIRST; idx < SCALER_ENUM_LAST; idx++)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_scaler_mode), scalers[idx].scaler_name);

    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_scaler_mode), SCALER_ENUM_FIRST);

    // Attach to table and show the combo
    gtk_box_pack_start (GTK_BOX (main_vbox), combo_scaler_mode, FALSE, FALSE, 0);
    gtk_widget_show (combo_scaler_mode);

    // Connect the changed signal to update the scaler mode
    g_signal_connect (combo_scaler_mode,
                      "changed",
                      G_CALLBACK (on_combo_scaler_mode_changed),
                      &scaler_mode);

    // Then connect a second signal to trigger a preview update
    g_signal_connect_swapped (combo_scaler_mode,
                              "changed",
                              G_CALLBACK (gimp_preview_invalidate),
                              preview);


  gtk_widget_show (dialog);

  run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);


  gtk_widget_destroy (dialog);

  return run;
}



// Handler : "changed" for SCALER MODE combo box
// callback_data not used currently
static void on_combo_scaler_mode_changed (GtkComboBox *combo, gpointer callback_data)
{

    // TODO: stop using global var scaler_mode?
    gchar *selected_string = gtk_combo_box_text_get_active_text( GTK_COMBO_BOX_TEXT(combo) );
    gint i;

    for (i=0; i < SCALER_ENUM_LAST; i++) {
        // If the mode string matched the one in the combo, select it as the current mode
        if (!(g_strcmp0(selected_string, scalers[i].scaler_name)))
          scaler_mode = i;
    }

    // TODO: move the preview_scaled resize call into here?
}



/*******************************************************/
/*                    APPLY SCALER                     */
/*******************************************************/

// TODO: This would be less brittle and easier to understand if
//       preview vs. apply was either seperated out or abstracted.

static glong preview_last_size;

void pixel_art_scalers_run (GimpDrawable *drawable, GimpPreview  *preview)
{
    GimpPixelRgn src_rgn;
    gint         bpp;
    // gint has_alpha;
    gint         width, height;

    gint         x, y;
    guint scale_factor;

    uint32_t     * p_srcbuf = NULL;
    uint32_t     * p_scaledbuf = NULL;

    glong        srcbuf_size = 0;
    glong        scaledbuf_size = 0;

    // Get the working image area for either the preview sub-window or the entire image
    if (preview) {
        gimp_preview_get_position (preview, &x, &y);
        gimp_preview_get_size (preview, &width, &height);

        gint cur_width, cur_height;
        gtk_widget_get_size_request (preview_scaled, &cur_width, &cur_height);
        scale_factor = scalers[scaler_mode].scale_factor;

        if ((cur_width * cur_height) !=
                (width * scale_factor * height * scale_factor)) {
//            printf("resize new=%d,%d  cur=%d,%d\n", width * scale_factor, height * scale_factor, cur_width, cur_height);
            gtk_widget_set_size_request (preview_scaled, width * scale_factor, height * scale_factor);

        // when set_size_request and then draw are called repeatedly on a preview_area
        // it results causes redraw glitching in the surrounding scrolled_window region
        // Calling set_max_size appears to fix this
        // (though it may be treating the symptom and not the cause of the glitching)
        gimp_preview_area_set_max_size(GIMP_PREVIEW_AREA (preview_scaled),
                                        width * scale_factor,
                                        height * scale_factor);


        }


    }
    else if (! gimp_drawable_mask_intersect (drawable->drawable_id,
                                             &x, &y, &width, &height)) {
        return;
    }

    // Get bit depth and alpha mask status
    bpp = drawable->bpp;
    // has_alpha = gimp_drawable_has_alpha (drawable->drawable_id);

    // Allocate a working buffer to copy the source image into - always RGBA 4BPP
    srcbuf_size = width * height * BYTE_SIZE_RGBA_4BPP;
    // TODO: WARNING Shouldn this be gint to ensure correct uint32_t byte alignment?
    p_srcbuf = (uint32_t *) g_new (guchar, srcbuf_size);


    // FALSE, FALSE : region will be used to read the actual drawable datas
    // Initialize source pixel region with drawable
    gimp_pixel_rgn_init (&src_rgn,
                         drawable,
                         x, y,
                         width, height,
                         FALSE, FALSE);

    // Copy source image to working buffer
    gimp_pixel_rgn_get_rect (&src_rgn,
                             (guchar *) p_srcbuf,
                             x, y, width, height);


    // Add alpha channel byte if needed (scalers expect 4BPP RGBA)
    if (bpp == BYTE_SIZE_RGB_3BPP)  // i.e. !has_alpha
        buffer_add_alpha_byte((guchar *) p_srcbuf, srcbuf_size);

    // ====== BEGIN SCALER ======
    // TODO: move to function
    // TODO: cache output to speed up redraws when output window gets panned around

// TODO: local scaler mode = GLOBAL?
    scale_factor = scalers[scaler_mode].scale_factor;


    // Allocate output buffer for the results
    // guchar = unsigned 8 bits, guint32 = unsigned 32 bits, uint32_t = unsigned 32 bits
    scaledbuf_size = width * scale_factor * height * scale_factor * sizeof(uint32_t);
    // TODO: WARNING is this overallocating? Above uses sizeof uint32_t, which seems to imply guchar, maybe reduce allocation size
    p_scaledbuf = (uint32_t *) g_new (guint, scaledbuf_size);

    if (p_scaledbuf) {

        // Expects 4BPP RGBA in p_srcbuf (outputs same in p_scaledbuf)
        filter_apply (scaler_mode,
                      p_srcbuf,
                      p_scaledbuf,
                      (int) width, (int) height);
    }

    // ====== END SCALER ======

    // Filter is done, apply the update
    if (preview) {

        // Draw scaled image onto preview area
        //   NOTE: requires access to a glboal var (preview_scaled)
        //   Preview expects 4BPP RGBA

//        gtk_widget_set_size_request (preview_scaled, width * scale_factor, height * scale_factor);

//    printf("---> redraw new=%d,%d\n", width * scale_factor, height * scale_factor);

        gimp_preview_area_draw (GIMP_PREVIEW_AREA (preview_scaled),
                                0, 0,
                                width * scale_factor,
                                height * scale_factor,
                                GIMP_RGBA_IMAGE,
                                (guchar *) p_scaledbuf,
                                width * scale_factor * BYTE_SIZE_RGBA_4BPP);
    }
    else
    {

        // Remove the alpha byte that was added if the source image was 3BPP RGB
        if (bpp == 3)  // i.e. !has_alpha
            buffer_remove_alpha_byte((guchar *) p_scaledbuf, scaledbuf_size);

        // Apply image result with full resize
        resize_image_and_apply_changes(drawable,
                                       (guchar *)p_scaledbuf,
                                       scale_factor);
    }

    // Free the working buffer
    g_free (p_srcbuf);

    if (p_scaledbuf)
        g_free(p_scaledbuf);
}




// Resizes image and then draws the newly scaled output onto it
// Params:
// * GimpDrawable  : from source image
// * guchar * buffer : scaled output
// * guint    scale_factor : image scale multiplier
static void resize_image_and_apply_changes(GimpDrawable * drawable, guchar * p_scaledbuf, guint scale_factor)
{
    GimpPixelRgn  dest_rgn;
    gint          x,y, width, height;
    GimpDrawable  * resized_drawable;

    if (! gimp_drawable_mask_intersect (drawable->drawable_id,
                                         &x, &y, &width, &height))
        return;

    // == START UNDO GROUPING
    gimp_image_undo_group_start(gimp_item_get_image(drawable->drawable_id));

    // Resize image
    if (gimp_image_resize(gimp_item_get_image(drawable->drawable_id),
                          width * scale_factor,
                          height * scale_factor,
                          0,0))
    {

        // Resize the current layer to match the resized image
        gimp_layer_resize_to_image_size( gimp_image_get_active_layer(
                                           gimp_item_get_image(drawable->drawable_id) ) );


        // Get a new drawable from the resized layer/image
        resized_drawable = gimp_drawable_get( gimp_image_get_active_drawable(
                                                gimp_item_get_image(drawable->drawable_id) ) );


        // Initialize destination pixel region with drawable
        // TRUE,  TRUE  : region will be used to write to the shadow tiles
        //                i.e. make changes that will be written back to source tiles
        gimp_pixel_rgn_init (&dest_rgn,
                             resized_drawable,
                             0, 0,
                             width * scale_factor,
                             height * scale_factor,
                             TRUE, TRUE);

        // Copy the scaled buffer to the shadow image
        gimp_pixel_rgn_set_rect (&dest_rgn,
                                 (guchar *) p_scaledbuf,
                                 0,0,
                                 width * scale_factor,
                                 height * scale_factor);

        // Apply the changes to the image (merge shadow, update drawable)
        gimp_drawable_flush (resized_drawable);
        gimp_drawable_merge_shadow (resized_drawable->drawable_id, TRUE);
        gimp_drawable_update (resized_drawable->drawable_id, 0, 0, width * scale_factor, height * scale_factor);

        // Free the extra resized drawable
        gimp_drawable_detach (resized_drawable);
    }

    // == END GROUPING
    gimp_image_undo_group_end(gimp_item_get_image(drawable->drawable_id));
}



static void filter_apply(int scaler_mode, uint32_t * p_srcbuf, uint32_t * p_destbuf, int width, int height) {

    if (scaler_mode < SCALER_ENUM_LAST) {

        // Call the requested scaler function
        scalers[scaler_mode].scaler_function( (uint32_t*) p_srcbuf,
                                              (uint32_t*) p_destbuf,
                                                    (int) width,
                                                    (int) height);
    }
}


static void buffer_add_alpha_byte(guchar * p_srcbuf, glong srcbuf_size) {

    // Iterates through the buffer backward, from END to START
    // and remaps from RGB to RGBA (adds alpha byte)
    // --> Copies the buffer on top of itself, so ORDER IS IMPORTANT!

    guchar * p_3bpp = p_srcbuf + ((srcbuf_size / 4) *3) - 3; // Last pixel of 3BPP buffer
    guchar * p_4bpp = p_srcbuf + srcbuf_size -  4;          // Last pixel of 4BPP Buffer
    glong idx = 0;

    while(idx < srcbuf_size) {
        p_4bpp[3] = ALPHA_MASK_OPAQUE;  // Set alpha mask byte to 100% opaque / visible
        p_4bpp[2] = p_3bpp[2]; // copy B
        p_4bpp[1] = p_3bpp[1]; // copy G
        p_4bpp[0] = p_3bpp[0]; // copy R

        p_3bpp -= 3;  // Advance 3BPP image pointer to previous pixel
        p_4bpp -= 4;  // Advance 4BPP image pointer to previous pixel
        idx += 4;
    }
}


static void buffer_remove_alpha_byte(guchar * p_srcbuf, glong srcbuf_size) {

    // Iterates through the buffer forward, from START to END
    // and remaps from RGBA to RGB (removes alpha byte)
    // --> Copies the buffer on top of itself, so ORDER IS IMPORTANT!

    guchar * p_3bpp = p_srcbuf; // Last First of 3BPP buffer
    guchar * p_4bpp = p_srcbuf; // Last First of 4BPP Buffer
    glong idx = 0;

    while(idx < srcbuf_size) {
          p_3bpp[0] = p_4bpp[0]; // copy R
          p_3bpp[1] = p_4bpp[1]; // copy G
          p_3bpp[2] = p_4bpp[2]; // copy B

        p_3bpp += 3;  // Advance 3BPP image pointer to next pixel
        p_4bpp += 4;  // Advance 4BPP image pointer to next pixel
        idx += 4;
    }
}


