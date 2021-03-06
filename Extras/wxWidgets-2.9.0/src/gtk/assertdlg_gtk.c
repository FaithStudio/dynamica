/* ///////////////////////////////////////////////////////////////////////////
// Name:        assertdlg_gtk.c
// Purpose:     GtkAssertDialog
// Author:      Francesco Montorsi
// Id:          $Id: assertdlg_gtk.c 58913 2009-02-15 15:16:18Z FM $
// Copyright:   (c) 2006 Francesco Montorsi
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////// */

#ifdef VMS
#define XCheckIfEvent XCHECKIFEVENT
#endif

#include "wx/platform.h"
#include "wx/gtk/assertdlg_gtk.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <gtk/gtk.h>

/* For FILE */
#include <stdio.h>

#include <gtk/gtkexpander.h>


/* ----------------------------------------------------------------------------
   Constants
 ---------------------------------------------------------------------------- */

/*
   NB: when changing order of the columns also update the gtk_list_store_new() call
       in gtk_assert_dialog_create_backtrace_list_model() function
 */
#define STACKFRAME_LEVEL_COLIDX        0
#define FUNCTION_NAME_COLIDX           1
#define SOURCE_FILE_COLIDX             2
#define LINE_NUMBER_COLIDX             3
#define FUNCTION_ARGS_COLIDX           4




/* ----------------------------------------------------------------------------
   GtkAssertDialog helpers
 ---------------------------------------------------------------------------- */

GtkWidget *gtk_assert_dialog_add_button_to (GtkBox *box, const gchar *label,
                                            const gchar *stock, gint response_id)
{
    /* create the button */
    GtkWidget *button = gtk_button_new_with_mnemonic (label);
    GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);

#if GTK_CHECK_VERSION(2,6,0)
    if (!gtk_check_version (2, 6, 0))
    {
        /* add a stock icon inside it */
        GtkWidget *image = gtk_image_new_from_stock (stock, GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image (GTK_BUTTON (button), image);
    }
#endif

    /* add to the given (container) widget */
    if (box)
        gtk_box_pack_end (box, button, FALSE, TRUE, 8);

    return button;
}

GtkWidget *gtk_assert_dialog_add_button (GtkAssertDialog *dlg, const gchar *label,
                                         const gchar *stock, gint response_id)
{
    /* create the button */
    GtkWidget *button = gtk_assert_dialog_add_button_to (NULL, label, stock, response_id);

    /* add the button to the dialog's action area */
    gtk_dialog_add_action_widget (GTK_DIALOG (dlg), button, response_id);

    return button;
}

void gtk_assert_dialog_append_text_column (GtkWidget *treeview, const gchar *name, int index)
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (name, renderer,
                                                       "text", index, NULL);
    gtk_tree_view_insert_column (GTK_TREE_VIEW (treeview), column, index);
    gtk_tree_view_column_set_resizable (column, TRUE);
    gtk_tree_view_column_set_reorderable (column, TRUE);
}

GtkWidget *gtk_assert_dialog_create_backtrace_list_model ()
{
    GtkListStore *store;
    GtkWidget *treeview;

    /* create list store */
    store = gtk_list_store_new (5,
                                G_TYPE_UINT,        /* stack frame number */
                                G_TYPE_STRING,      /* function name      */
                                G_TYPE_STRING,      /* source file name   */
                                G_TYPE_STRING,      /* line number        */
                                G_TYPE_STRING);     /* function arguments */

    /* create the tree view */
    treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL(store));
    g_object_unref (store);
    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);

    /* append columns */
    gtk_assert_dialog_append_text_column(treeview, "#", STACKFRAME_LEVEL_COLIDX);
    gtk_assert_dialog_append_text_column(treeview, "Function name", FUNCTION_NAME_COLIDX);
    gtk_assert_dialog_append_text_column(treeview, "Function args", FUNCTION_ARGS_COLIDX);
    gtk_assert_dialog_append_text_column(treeview, "Source file", SOURCE_FILE_COLIDX);
    gtk_assert_dialog_append_text_column(treeview, "Line #", LINE_NUMBER_COLIDX);

    return treeview;
}

void gtk_assert_dialog_process_backtrace (GtkAssertDialog *dlg)
{
    /* set busy cursor */
    GdkWindow *parent = GTK_WIDGET(dlg)->window;
    GdkCursor *cur = gdk_cursor_new (GDK_WATCH);
    gdk_window_set_cursor (parent, cur);
    gdk_flush ();

    (*dlg->callback)(dlg->userdata);

    /* toggle busy cursor */
    gdk_window_set_cursor (parent, NULL);
    gdk_cursor_unref (cur);
}



/* ----------------------------------------------------------------------------
   GtkAssertDialog signal handlers
 ---------------------------------------------------------------------------- */

void gtk_assert_dialog_expander_callback (GtkWidget *widget, GtkAssertDialog *dlg)
{
    /* status is not yet updated so we need to invert it to get the new one */
    gboolean expanded = !gtk_expander_get_expanded (GTK_EXPANDER(dlg->expander));
    gtk_window_set_resizable (GTK_WINDOW (dlg), expanded);

    if (dlg->callback == NULL)      /* was the backtrace already processed? */
        return;

    gtk_assert_dialog_process_backtrace (dlg);

    /* mark the work as done (so that next activate we won't call the callback again) */
    dlg->callback = NULL;
}

void gtk_assert_dialog_save_backtrace_callback (GtkWidget *widget, GtkAssertDialog *dlg)
{
    GtkWidget *dialog;

    dialog = gtk_file_chooser_dialog_new ("Save assert info to file", GTK_WINDOW(dlg),
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                          NULL);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
    {
        char *filename, *msg, *backtrace;
        FILE *fp;

        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        if ( filename )
        {
            msg = gtk_assert_dialog_get_message (dlg);
            backtrace = gtk_assert_dialog_get_backtrace (dlg);

            /* open the file and write all info inside it */
            fp = fopen (filename, "w");
            if (fp)
            {
                fprintf (fp, "ASSERT INFO:\n%s\n\nBACKTRACE:\n%s", msg, backtrace);
                fclose (fp);
            }

            g_free (filename);
            g_free (msg);
            g_free (backtrace);
        }
    }

    gtk_widget_destroy (dialog);
}

void gtk_assert_dialog_copy_callback (GtkWidget *widget, GtkAssertDialog *dlg)
{
    char *msg, *backtrace;
    GtkClipboard *clipboard;
    GString *str;

    msg = gtk_assert_dialog_get_message (dlg);
    backtrace = gtk_assert_dialog_get_backtrace (dlg);

    /* combine both in a single string */
    str = g_string_new("");
    g_string_printf (str, "ASSERT INFO:\n%s\n\nBACKTRACE:\n%s\n\n", msg, backtrace);

    /* copy everything in default clipboard */
    clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text (clipboard, str->str, str->len);

    /* copy everything in primary clipboard too */
    clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
    gtk_clipboard_set_text (clipboard, str->str, str->len);

    g_free (msg);
    g_free (backtrace);
    g_string_free (str, TRUE);
}

void gtk_assert_dialog_continue_callback (GtkWidget *widget, GtkAssertDialog *dlg)
{
    gint response =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(dlg->shownexttime)) ?
            GTK_ASSERT_DIALOG_CONTINUE : GTK_ASSERT_DIALOG_CONTINUE_SUPPRESSING;

    gtk_dialog_response (GTK_DIALOG(dlg), response);
}


/* ----------------------------------------------------------------------------
   GtkAssertDialogClass implementation
 ---------------------------------------------------------------------------- */

static void     gtk_assert_dialog_init              (GtkAssertDialog        *self);
static void     gtk_assert_dialog_class_init        (GtkAssertDialogClass *klass);


GtkType gtk_assert_dialog_get_type (void)
{
    static GtkType assert_dialog_type = 0;

    if (!assert_dialog_type)
    {
        const GTypeInfo assert_dialog_info =
        {
            sizeof (GtkAssertDialogClass),
            NULL,           /* base_init */
            NULL,           /* base_finalize */
            (GClassInitFunc) gtk_assert_dialog_class_init,
            NULL,           /* class_finalize */
            NULL,           /* class_data */
            sizeof (GtkAssertDialog),
            16,             /* n_preallocs */
            (GInstanceInitFunc) gtk_assert_dialog_init,
            NULL
        };
        assert_dialog_type = g_type_register_static (GTK_TYPE_DIALOG, "GtkAssertDialog", &assert_dialog_info, (GTypeFlags)0);
    }

    return assert_dialog_type;
}

void gtk_assert_dialog_class_init(GtkAssertDialogClass *klass)
{
    /* no special initializations required */
}

void gtk_assert_dialog_init(GtkAssertDialog *dlg)
{
    GtkWidget *continuebtn;

    {
        GtkWidget *vbox, *hbox, *image;

        /* start the main vbox */
        gtk_widget_push_composite_child ();
        vbox = gtk_vbox_new (FALSE, 8);
        gtk_container_set_border_width (GTK_CONTAINER(vbox), 8);
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), vbox, TRUE, TRUE, 5);


        /* add the icon+message hbox */
        hbox = gtk_hbox_new (FALSE, 0);
        gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

        /* icon */
        image = gtk_image_new_from_stock (GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_DIALOG);
        gtk_box_pack_start (GTK_BOX(hbox), image, FALSE, FALSE, 12);

        {
            GtkWidget *vbox2, *info;

            /* message */
            vbox2 = gtk_vbox_new (FALSE, 0);
            gtk_box_pack_start (GTK_BOX (hbox), vbox2, TRUE, TRUE, 0);
            info = gtk_label_new ("An assertion failed!");
            gtk_box_pack_start (GTK_BOX(vbox2), info, TRUE, TRUE, 8);

            /* assert message */
            dlg->message = gtk_label_new (NULL);
            gtk_label_set_selectable (GTK_LABEL (dlg->message), TRUE);
            gtk_label_set_line_wrap (GTK_LABEL (dlg->message), TRUE);
            gtk_label_set_justify (GTK_LABEL (dlg->message), GTK_JUSTIFY_LEFT);
            gtk_widget_set_size_request (GTK_WIDGET(dlg->message), 450, -1);

            gtk_box_pack_end (GTK_BOX(vbox2), GTK_WIDGET(dlg->message), TRUE, TRUE, 8);
        }

        /* add the expander */
        dlg->expander = gtk_expander_new_with_mnemonic ("Back_trace:");
        gtk_box_pack_start (GTK_BOX(vbox), dlg->expander, TRUE, TRUE, 0);
        g_signal_connect (GTK_EXPANDER(dlg->expander), "activate",
                            G_CALLBACK(gtk_assert_dialog_expander_callback), dlg);
    }

    {
        GtkWidget *hbox, *vbox, *button, *sw;

        /* create expander's vbox */
        vbox = gtk_vbox_new (FALSE, 0);
        gtk_container_add (GTK_CONTAINER (dlg->expander), vbox);

        /* add a scrollable window under the expander */
        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_ETCHED_IN);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        gtk_box_pack_start (GTK_BOX(vbox), sw, TRUE, TRUE, 8);

        /* add the treeview to the scrollable window */
        dlg->treeview = gtk_assert_dialog_create_backtrace_list_model ();
        gtk_widget_set_size_request (GTK_WIDGET(dlg->treeview), -1, 180);
        gtk_container_add (GTK_CONTAINER (sw), dlg->treeview);

        /* create button's hbox */
        hbox = gtk_hbutton_box_new ();
        gtk_box_pack_end (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
        gtk_button_box_set_layout (GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);

        /* add the buttons */
        button = gtk_assert_dialog_add_button_to (GTK_BOX(hbox), "Save to _file",
                                                GTK_STOCK_SAVE, GTK_RESPONSE_NONE);
        g_signal_connect (button, "clicked",
                            G_CALLBACK(gtk_assert_dialog_save_backtrace_callback), dlg);

        button = gtk_assert_dialog_add_button_to (GTK_BOX(hbox), "Copy to clip_board",
                                                  GTK_STOCK_COPY, GTK_RESPONSE_NONE);
        g_signal_connect (button, "clicked", G_CALLBACK(gtk_assert_dialog_copy_callback), dlg);
    }

    /* add the checkbutton */
    dlg->shownexttime = gtk_check_button_new_with_mnemonic("Show this _dialog the next time");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(dlg->shownexttime), TRUE);
    gtk_box_pack_end (GTK_BOX(GTK_DIALOG(dlg)->action_area), dlg->shownexttime, FALSE, TRUE, 8);

    /* add the stop button */
    gtk_assert_dialog_add_button (dlg, "_Stop", GTK_STOCK_QUIT, GTK_ASSERT_DIALOG_STOP);

    /* add the continue button */
    continuebtn = gtk_assert_dialog_add_button (dlg, "_Continue", GTK_STOCK_YES, GTK_ASSERT_DIALOG_CONTINUE);
    gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_ASSERT_DIALOG_CONTINUE);
    g_signal_connect (continuebtn, "clicked", G_CALLBACK(gtk_assert_dialog_continue_callback), dlg);

    /* complete creation */
    dlg->callback = NULL;
    dlg->userdata = NULL;

    /* the resizeable property of this window is modified by the expander:
       when it's collapsed, the window must be non-resizeable! */
    gtk_window_set_resizable (GTK_WINDOW (dlg), FALSE);
    gtk_widget_pop_composite_child ();
    gtk_widget_show_all (GTK_WIDGET(dlg));
}



/* ----------------------------------------------------------------------------
   GtkAssertDialog public API
 ---------------------------------------------------------------------------- */

gchar *gtk_assert_dialog_get_message (GtkAssertDialog *dlg)
{
    /* NOTES:
       1) returned string must g_free()d !
       2) Pango markup is automatically stripped off by GTK
    */
    return g_strdup (gtk_label_get_text (GTK_LABEL(dlg->message)));
}

gchar *gtk_assert_dialog_get_backtrace (GtkAssertDialog *dlg)
{
    gchar *function, *arguments, *sourcefile, *linenum;
    guint count;

    GtkTreeModel *model;
    GtkTreeIter iter;
    GString *string;

    g_return_val_if_fail (GTK_IS_ASSERT_DIALOG (dlg), NULL);
    model = gtk_tree_view_get_model (GTK_TREE_VIEW(dlg->treeview));
    string = g_string_new("");

    /* iterate over the list */
    if (!gtk_tree_model_get_iter_first (model, &iter))
        return NULL;

    do
    {
        /* append this stack frame's info to the string */
        gtk_tree_model_get (model, &iter,
                            STACKFRAME_LEVEL_COLIDX, &count,
                            FUNCTION_NAME_COLIDX, &function,
                            FUNCTION_ARGS_COLIDX, &arguments,
                            SOURCE_FILE_COLIDX, &sourcefile,
                            LINE_NUMBER_COLIDX, &linenum,
                            -1);

        g_string_append_printf (string, "[%u] %s(%s)",
                                count, function, arguments);
        if (sourcefile[0] != '\0')
            g_string_append_printf (string, " %s", sourcefile);
        if (linenum[0] != '\0')
            g_string_append_printf (string, ":%s", linenum);
        g_string_append (string, "\n");

        g_free (function);
        g_free (arguments);
        g_free (sourcefile);
        g_free (linenum);

    } while (gtk_tree_model_iter_next (model, &iter));

    /* returned string must g_free()d */
    return g_string_free (string, FALSE);
}

void gtk_assert_dialog_set_message(GtkAssertDialog *dlg, const gchar *msg)
{
    /* prepend and append the <b> tag
       NOTE: g_markup_printf_escaped() is not used because it's available
             only for glib >= 2.4 */
    gchar *escaped_msg = g_markup_escape_text (msg, -1);
    gchar *decorated_msg = g_strdup_printf ("<b>%s</b>", escaped_msg);

    g_return_if_fail (GTK_IS_ASSERT_DIALOG (dlg));
    gtk_label_set_markup (GTK_LABEL(dlg->message), decorated_msg);

    g_free (decorated_msg);
    g_free (escaped_msg);
}

void gtk_assert_dialog_set_backtrace_callback(GtkAssertDialog *assertdlg,
                                              GtkAssertDialogStackFrameCallback callback,
                                              void *userdata)
{
    assertdlg->callback = callback;
    assertdlg->userdata = userdata;
}

void gtk_assert_dialog_append_stack_frame(GtkAssertDialog *dlg,
                                          const gchar *function,
                                          const gchar *arguments,
                                          const gchar *sourcefile,
                                          guint line_number)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GString *linenum;
    gint count;

    g_return_if_fail (GTK_IS_ASSERT_DIALOG (dlg));
    model = gtk_tree_view_get_model (GTK_TREE_VIEW(dlg->treeview));

    /* how many items are in the list up to now ? */
    count = gtk_tree_model_iter_n_children (model, NULL);

    linenum = g_string_new("");
    if ( line_number != 0 )
        g_string_printf (linenum, "%u", line_number);

    /* add data to the list store */
    gtk_list_store_append (GTK_LIST_STORE(model), &iter);
    gtk_list_store_set (GTK_LIST_STORE(model), &iter,
                        STACKFRAME_LEVEL_COLIDX, count+1,     /* start from 1 and not from 0 */
                        FUNCTION_NAME_COLIDX, function,
                        FUNCTION_ARGS_COLIDX, arguments,
                        SOURCE_FILE_COLIDX, sourcefile,
                        LINE_NUMBER_COLIDX, linenum->str,
                        -1);

    g_string_free (linenum, TRUE);
}

GtkWidget *gtk_assert_dialog_new(void)
{
    GtkAssertDialog *dialog = g_object_new (GTK_TYPE_ASSERT_DIALOG, NULL);

    return GTK_WIDGET (dialog);
}

#ifdef __cplusplus
}
#endif /* __cplusplus */
