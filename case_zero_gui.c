/* CASE ZERO V5 — Native C + GTK3 desktop frontend (Linux).
   Copyright: project owner. No Python or web runtime required. */
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COLOR_BG "#080e1b"

typedef struct {
    GtkWidget *win, *transcript, *choices, *entry, *send, *clock_label;
    GtkWidget *elapsed, *case_title, *status, *hint, *scroll;
    GtkTextBuffer *buffer;
    GSubprocess *proc;
    GDataInputStream *reader;
    GOutputStream *writer;
    gboolean waiting, paused, text_input, is_first_prompt;
    gint low, high, pending_mode;
    gint64 started;
    gchar *engine_path;
    gchar *last_lines[100];
    guint line_count;
} App;
static App app = {0};
static const gchar *modes[] = {"COMMAND CENTER", "BLACKOUT CONSPIRACY", "MAFIA: TRUST NO ONE",
 "MIDNIGHT MURDER", "ESCAPE THE DARKNESS", "SUDOKU SECURITY LOCK", "CASE HISTORY", "DETECTIVE CASE FILES"};

static void append(const char *str, const char *tag) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(app.buffer, &end);
    if (tag) gtk_text_buffer_insert_with_tags_by_name(app.buffer,&end,str,-1,tag,NULL);
    else gtk_text_buffer_insert(app.buffer,&end,str,-1);
    gtk_text_buffer_get_end_iter(app.buffer,&end);
    gtk_text_buffer_insert(app.buffer,&end,"\n",1);
    gtk_text_buffer_get_end_iter(app.buffer,&end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(app.transcript),&end,0.0,FALSE,0,0);
}
static void empty_choices(void) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(app.choices));
    for(GList *i=children;i;i=i->next) gtk_widget_destroy(GTK_WIDGET(i->data));
    g_list_free(children);
}
static void reset_lines(void) {
    for(guint i=0;i<app.line_count;i++) g_free(app.last_lines[i]);
    app.line_count=0;
}
static void record_line(const gchar *s) {
    if(app.line_count==100){g_free(app.last_lines[0]);memmove(app.last_lines,app.last_lines+1,99*sizeof(gchar*));app.line_count=99;}
    app.last_lines[app.line_count++]=g_strdup(s);
}
static gchar *strip_ansi(const gchar *raw) {
    GString *r=g_string_new("");
    for(const unsigned char *p=(const unsigned char*)raw;*p;p++) {
        if(*p==27 && p[1]=='[') {p+=2;while(*p && !((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')))p++;if(!*p)break;}
        else if(*p!='\r' && *p!='\a')g_string_append_c(r,(char)*p);
    }
    return g_string_free(r,FALSE);
}
static void set_status(const gchar *s) { gtk_label_set_text(GTK_LABEL(app.status),s); }
static void send_choice(const gchar *value) {
    if(!app.waiting || !app.writer || !app.proc)return;
    gchar *line=g_strconcat(value,"\n",NULL);
    GError *err=NULL;
    if(!g_output_stream_write_all(app.writer,line,strlen(line),NULL,NULL,&err)) {
        append(err?err->message:"Could not send input.","error");g_clear_error(&err);
    } else {
        app.waiting=FALSE;app.text_input=FALSE;
        gtk_entry_set_text(GTK_ENTRY(app.entry),"");
        gtk_label_set_text(GTK_LABEL(app.hint),"INVESTIGATING...");
        set_status("● ANALYZING EVIDENCE");
        empty_choices();
    }
    g_free(line);
}
static void choice_clicked(GtkButton *b,gpointer data) {(void)b;send_choice((const gchar*)data);}
static void add_choice(const gchar *label,const gchar *value) {
    GtkWidget *button=gtk_button_new_with_label(label);
    gtk_style_context_add_class(gtk_widget_get_style_context(button),"choice");
    g_signal_connect_data(button,"clicked",G_CALLBACK(choice_clicked),g_strdup(value),(GClosureNotify)g_free,0);
    GtkWidget *caption=gtk_bin_get_child(GTK_BIN(button));
    if(GTK_IS_LABEL(caption)){
        gtk_label_set_line_wrap(GTK_LABEL(caption),TRUE);
        gtk_label_set_line_wrap_mode(GTK_LABEL(caption),PANGO_WRAP_WORD_CHAR);
        gtk_label_set_max_width_chars(GTK_LABEL(caption),28);
    }
    gtk_flow_box_insert(GTK_FLOW_BOX(app.choices),button,-1);
    gtk_widget_show(button);
}
static void build_choices(void) {
    empty_choices();
    if(app.paused){ add_choice("CONTINUE  ▶",""); return; }
    gboolean seen[101]={FALSE};gint found=0;
    // Interpret the latest numbered entries when possible, otherwise numeric action buttons.
    for(gint i=(gint)app.line_count-1;i>=0 && found<12;i--) {
        gchar *s=g_strstrip(g_strdup(app.last_lines[i]));
        gchar **parts=g_strsplit(s,"|",-1);
        for(gint k=0;parts[k];k++) {
            gchar *part=g_strstrip(parts[k]);
            gchar *end=NULL;long v=strtol(part,&end,10);
            if(v>=app.low && v<=app.high && v>=0 && v<=100 && end!=part && (*end=='.'||*end==')') && !seen[v]) {
                gchar *title=g_strstrip(end+1);
                if(strlen(title)>1 && strlen(title)<68) {
                    gchar *lbl=g_strdup_printf("%02ld   %s",v,title);
                    gchar *val=g_strdup_printf("%ld",v);
                    add_choice(lbl,val);g_free(lbl);g_free(val);
                    seen[v]=TRUE;found++;
                }
            }
        }
        g_strfreev(parts);g_free(s);
        if(found && seen[app.low]) break;
    }
    if(!found && app.high-app.low<=11)for(gint i=app.low;i<=app.high;i++){
        gchar *val=g_strdup_printf("%d",i);add_choice(val,val);g_free(val);
    }
}
static void on_engine_line(const gchar *line){
    if(strstr(line,"@@CZ_SCREEN@@")){
        gtk_text_buffer_set_text(app.buffer,"",-1);reset_lines();empty_choices();app.waiting=FALSE;return;
    }
    gint lo=0,hi=0;
    if(sscanf(line,"@@CZ_INPUT:%d:%d@@",&lo,&hi)==2){
        app.low=lo;app.high=hi;app.paused=FALSE;app.text_input=FALSE;app.waiting=TRUE;
        if(app.is_first_prompt && app.pending_mode>=1 && app.pending_mode<=7){
            gchar *v=g_strdup_printf("%d",app.pending_mode);app.pending_mode=0;app.is_first_prompt=FALSE;send_choice(v);g_free(v);return;
        }
        app.is_first_prompt=FALSE;
        gchar *msg=g_strdup_printf("ENTER A NUMBER FROM %d TO %d",lo,hi);
        gtk_label_set_text(GTK_LABEL(app.hint),msg);g_free(msg);
        set_status("● DECISION REQUIRED");build_choices();return;
    }
    if(strstr(line,"@@CZ_TEXT@@")){
        app.waiting=TRUE;app.paused=FALSE;app.text_input=TRUE;
        gtk_label_set_text(GTK_LABEL(app.hint),"TYPE YOUR DETECTIVE NAME AND CONFIRM");
        set_status("● DETECTIVE PROFILE");empty_choices();return;
    }
    if(strstr(line,"@@CZ_PAUSE@@")){
        app.waiting=TRUE;app.paused=TRUE;
        set_status("● CONTINUE STORY");gtk_label_set_text(GTK_LABEL(app.hint),"ADVANCE TO NEXT SCENE");build_choices();return;
    }
    gchar *clean=strip_ansi(line);
    if(g_str_has_prefix(g_strstrip(clean),"======================================================") ||
       strstr(clean,"C A S E  Z E R O") || strstr(clean,"THE MYSTERY FILES | KARACHI")) {g_free(clean);return;}
    record_line(clean);
    gchar *upper=g_ascii_strup(clean,-1);
    const gchar *tag=NULL;
    if(strstr(upper,"CORRECT")||strstr(upper,"UNLOCKED")||strstr(upper,"RESTORED"))tag="success";
    else if(strstr(upper,"FAILED")||strstr(upper,"WRONG")||strstr(upper,"ELIMINATED"))tag="error";
    else if(strstr(upper,"EPISODE")||strstr(upper,"SUDOKU")||strstr(upper,"NIGHT ")||strstr(upper,"THE VOTE"))tag="accent";
    if(strlen(clean)>0)append(clean,tag);
    if(strstr(upper,"BLACKOUT CONSPIRACY")||strstr(upper,"EPISODE 1:"))gtk_label_set_text(GTK_LABEL(app.case_title),modes[1]);
    else if(strstr(upper,"MAFIA: TRUST")||g_str_has_prefix(upper,"NIGHT "))gtk_label_set_text(GTK_LABEL(app.case_title),modes[2]);
    else if(strstr(upper,"MIDNIGHT MURDER"))gtk_label_set_text(GTK_LABEL(app.case_title),modes[3]);
    else if(strstr(upper,"ESCAPE THE DARKNESS"))gtk_label_set_text(GTK_LABEL(app.case_title),modes[4]);
    else if(strstr(upper,"SUDOKU SECURITY LOCK"))gtk_label_set_text(GTK_LABEL(app.case_title),modes[5]);
    else if(strstr(upper,"DETECTIVE CASE FILES"))gtk_label_set_text(GTK_LABEL(app.case_title),modes[7]);
    g_free(upper);g_free(clean);
}
static void read_next(void);
static void read_done(GObject *src,GAsyncResult *result,gpointer data){
    (void)data;GError *err=NULL;gsize len=0;
    gchar *line=g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(src),result,&len,&err);
    if(G_DATA_INPUT_STREAM(src)!=app.reader){g_free(line);g_clear_error(&err);return;}
    if(err){append(err->message,"error");g_clear_error(&err);return;}
    if(!line){app.waiting=FALSE;set_status("● SESSION COMPLETE");
        gtk_label_set_text(GTK_LABEL(app.hint),"SELECT A MODE TO RESTART");return;}
    on_engine_line(line);g_free(line);read_next();
}
static void read_next(void){
    if(app.reader)g_data_input_stream_read_line_async(app.reader,G_PRIORITY_DEFAULT,NULL,read_done,NULL);
}
static void stop_engine(void){
    app.waiting=FALSE;app.writer=NULL;
    if(app.proc){g_subprocess_force_exit(app.proc);g_object_unref(app.proc);app.proc=NULL;}
    if(app.reader){g_object_unref(app.reader);app.reader=NULL;}
}
static void launch_engine(gint mode){
    stop_engine();reset_lines();empty_choices();
    gtk_text_buffer_set_text(app.buffer,"",-1);
    app.pending_mode=mode;app.is_first_prompt=TRUE;app.paused=FALSE;app.text_input=FALSE;
    app.started=g_get_monotonic_time();
    gtk_label_set_text(GTK_LABEL(app.case_title),modes[mode]);
    gtk_label_set_text(GTK_LABEL(app.hint),"STARTING C GAME ENGINE...");
    set_status("● INITIALIZING");
    GError *err=NULL;
    GSubprocessLauncher *launcher=g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_STDIN_PIPE|G_SUBPROCESS_FLAGS_STDOUT_PIPE|G_SUBPROCESS_FLAGS_STDERR_MERGE);
    gchar *dir=g_path_get_dirname(app.engine_path);
    g_subprocess_launcher_set_cwd(launcher,dir);
    app.proc=g_subprocess_launcher_spawn(launcher,&err,app.engine_path,NULL);
    g_object_unref(launcher);g_free(dir);
    if(!app.proc){append(err?err->message:"Engine executable could not start.","error");
        append("Compile case_zero.c using the instructions in README.md.","error");
        set_status("● ENGINE NOT FOUND");g_clear_error(&err);return;}
    app.writer=g_subprocess_get_stdin_pipe(app.proc);
    app.reader=g_data_input_stream_new(g_subprocess_get_stdout_pipe(app.proc));
    read_next();
}
static void mode_button(GtkButton *b,gpointer data){(void)b;launch_engine(GPOINTER_TO_INT(data));}
static void on_confirm(GtkWidget *w,gpointer data){(void)w;(void)data;
    if(!app.waiting)return;
    if(app.paused){send_choice("");return;}
    const gchar *s=gtk_entry_get_text(GTK_ENTRY(app.entry));
    if(app.text_input){
        if(strlen(s)>38 || strchr(s,'\n') || strchr(s,'|')){
            gtk_label_set_text(GTK_LABEL(app.hint),"NAME MUST BE UNDER 39 CHARACTERS, NO | SYMBOL");return;
        }
        send_choice(s);return;
    }
    gchar *end;long n=strtol(s,&end,10);
    if(end==s || *end!='\0' || n<app.low || n>app.high){
        gtk_label_set_text(GTK_LABEL(app.hint),"INVALID OPTION — CHECK THE RANGE");return;
    }
    send_choice(s);
}
static gboolean tick(gpointer unused){(void)unused;
    GDateTime *utc=g_date_time_new_now_utc();
    GDateTime *pkt=g_date_time_add_hours(utc,5);
    gchar *stamp=g_date_time_format(pkt,"%I:%M:%S %p");
    gchar *full=g_strconcat(stamp," PKT",NULL);
    gtk_label_set_text(GTK_LABEL(app.clock_label),full);
    gint64 secs=(g_get_monotonic_time()-app.started)/1000000;
    if(secs<0)secs=0;
    gchar *elapsed=g_strdup_printf("SESSION %02"G_GINT64_FORMAT":%02"G_GINT64_FORMAT":%02"G_GINT64_FORMAT,
                                  secs/3600,(secs/60)%60,secs%60);
    gtk_label_set_text(GTK_LABEL(app.elapsed),elapsed);
    g_free(elapsed);g_free(full);g_free(stamp);g_date_time_unref(pkt);g_date_time_unref(utc);
    return G_SOURCE_CONTINUE;
}
static GtkWidget *label(const char *text,const char *style){
    GtkWidget *w=gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(w),0);
    if(style)gtk_style_context_add_class(gtk_widget_get_style_context(w),style);
    return w;
}
static GtkWidget *panel(GtkWidget *parent,int spacing,int padding){
    GtkWidget *b=gtk_box_new(GTK_ORIENTATION_VERTICAL,spacing);
    gtk_container_set_border_width(GTK_CONTAINER(b),padding);
    gtk_box_pack_start(GTK_BOX(parent),b,FALSE,FALSE,0);
    return b;
}
static void build_ui(void){
    app.win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app.win),"CASE ZERO V5  //  THE MYSTERY FILES  —  C EDITION");
    /* Match the actual usable desktop area rather than forcing 1320x820. */
    GdkDisplay *display=gdk_display_get_default();
    GdkMonitor *monitor=gdk_display_get_primary_monitor(display);
    if(!monitor && gdk_display_get_n_monitors(display)>0)
        monitor=gdk_display_get_monitor(display,0);
    GdkRectangle work={0,0,1100,720};
    if(monitor)gdk_monitor_get_workarea(monitor,&work);
    gtk_window_set_default_size(GTK_WINDOW(app.win),MAX(640,work.width*9/10),
                                MAX(460,work.height*9/10));
    gtk_window_set_position(GTK_WINDOW(app.win),GTK_WIN_POS_CENTER);
    gtk_window_set_geometry_hints(GTK_WINDOW(app.win),NULL,
           &(GdkGeometry){.min_width=640,.min_height=460},GDK_HINT_MIN_SIZE);
    g_signal_connect(app.win,"destroy",G_CALLBACK(gtk_main_quit),NULL);
    GtkCssProvider *css=gtk_css_provider_new();
    const char *style=
      "window {background:#080e1b;color:#e8f1ff;font-family:'Segoe UI',sans-serif;}"
      "label {color:#e8f1ff;}"
      ".sidebar {background:#101b2b;border-right:1px solid #2c3b51;}"
      ".sidebar-scroll, .choices-scroll {background:#101b2b;}"
      ".brand {font-size:23px;font-weight:900;color:#ffffff;}"
      ".eyebrow {font-size:11px;letter-spacing:2px;color:#62dce6;font-weight:bold;}"
      ".muted {font-size:11px;color:#97a9c1;}"
      ".main-title {font-size:23px;font-weight:bold;}"
      ".mode-title {font-size:21px;font-weight:900;color:white;}"
      ".clock {font-family:monospace;font-size:20px;font-weight:bold;color:#64e6ee;}"
      ".status {color:#ffce78;font-size:12px;font-weight:bold;}"
      ".card {background:#132138;border:1px solid #2b3d56;border-radius:12px;}"
      ".story {background:#111e30;color:#e8f0fa;font-family:monospace;font-size:13px;caret-color:#ffffff;}"
      "textview text {background:#111e30;color:#e8f0fa;}"
      "button {background:#1b304a;color:#eef5ff;border:1px solid #304661;border-radius:8px;padding:7px 9px;font-weight:bold;}"
      "button:hover {background:#2b5067;}"
      "button:active {background:#1d7181;}"
      ".mode {margin-bottom:6px;text-align:left;}"
      ".choice {padding:6px 9px;background:#163b4a;}"
      ".primary {background:#db3956;border-color:#db3956;color:#ffffff;padding:9px 15px;}"
      ".primary:hover {background:#f04c67;}"
      "entry {background:#172940;color:#ffffff;border:1px solid #344c66;border-radius:8px;padding:12px;font-size:16px;}";
    gtk_css_provider_load_from_data(css,style,-1,NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
    GtkWidget *root=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);gtk_container_add(GTK_CONTAINER(app.win),root);
    /* The entire navigation scrolls on smaller laptop displays. */
    GtkWidget *side_scroll=gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(side_scroll),
                                   GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(side_scroll,232,-1);
    gtk_style_context_add_class(gtk_widget_get_style_context(side_scroll),"sidebar-scroll");
    gtk_box_pack_start(GTK_BOX(root),side_scroll,FALSE,TRUE,0);
    GtkWidget *side=gtk_box_new(GTK_ORIENTATION_VERTICAL,7);
    gtk_style_context_add_class(gtk_widget_get_style_context(side),"sidebar");
    gtk_container_set_border_width(GTK_CONTAINER(side),12);
    gtk_container_add(GTK_CONTAINER(side_scroll),side);
    GtkWidget *head=label("◉  CASE ZERO","brand");gtk_box_pack_start(GTK_BOX(side),head,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(side),label("THE MYSTERY FILES   /   V5","eyebrow"),FALSE,FALSE,0);
    GtkWidget *sep=gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);gtk_box_pack_start(GTK_BOX(side),sep,FALSE,FALSE,13);
    gtk_box_pack_start(GTK_BOX(side),label("INVESTIGATION FILES","muted"),FALSE,FALSE,5);
    const char *short_names[]={"01   BLACKOUT CONSPIRACY","02   MAFIA: TRUST NO ONE","03   MIDNIGHT MURDER","04   ESCAPE THE DARKNESS","05   SUDOKU SECURITY LOCK","06   CASE HISTORY","07   DETECTIVE CASE FILES"};
    for(gint i=1;i<=7;i++){
        GtkWidget *btn=gtk_button_new_with_label(short_names[i-1]);
        gtk_widget_set_halign(btn,GTK_ALIGN_FILL);
        gtk_style_context_add_class(gtk_widget_get_style_context(btn),"mode");
        g_signal_connect(btn,"clicked",G_CALLBACK(mode_button),GINT_TO_POINTER(i));
        gtk_box_pack_start(GTK_BOX(side),btn,FALSE,FALSE,0);
    }
    GtkWidget *restart=gtk_button_new_with_label("↻   RESTART / MAIN MENU");
    g_signal_connect(restart,"clicked",G_CALLBACK(mode_button),GINT_TO_POINTER(0));
    gtk_box_pack_start(GTK_BOX(side),restart,FALSE,FALSE,18);
    GtkWidget *side_spacer=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    gtk_box_pack_start(GTK_BOX(side),side_spacer,TRUE,TRUE,0);
    gtk_box_pack_start(GTK_BOX(side),label("● ORIGINAL ENGINE: C11","eyebrow"),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(side),label("FRONTEND: C / GTK3","muted"),FALSE,FALSE,0);
    GtkWidget *main=gtk_box_new(GTK_ORIENTATION_VERTICAL,9);
    gtk_container_set_border_width(GTK_CONTAINER(main),12);
    gtk_box_pack_start(GTK_BOX(root),main,TRUE,TRUE,0);
    GtkWidget *top=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,15);
    gtk_box_pack_start(GTK_BOX(main),top,FALSE,FALSE,0);
    GtkWidget *titles=gtk_box_new(GTK_ORIENTATION_VERTICAL,5);
    gtk_box_pack_start(GTK_BOX(top),titles,TRUE,TRUE,0);
    gtk_box_pack_start(GTK_BOX(titles),label("INVESTIGATION CONSOLE","main-title"),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(titles),label("INTERACTIVE CASE MANAGEMENT  /  KARACHI","muted"),FALSE,FALSE,0);
    GtkWidget *right=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);gtk_box_pack_end(GTK_BOX(top),right,FALSE,FALSE,0);
    app.clock_label=label("12:00:00 AM PKT","clock");
    gtk_label_set_xalign(GTK_LABEL(app.clock_label),1);
    gtk_box_pack_start(GTK_BOX(right),app.clock_label,FALSE,FALSE,0);
    app.elapsed=label("SESSION 00:00:00","muted");gtk_label_set_xalign(GTK_LABEL(app.elapsed),1);
    gtk_box_pack_start(GTK_BOX(right),app.elapsed,FALSE,FALSE,0);
    GtkWidget *hero=panel(main,5,10);
    gtk_style_context_add_class(gtk_widget_get_style_context(hero),"card");
    gtk_box_pack_start(GTK_BOX(hero),label("ACTIVE CASE FILE","eyebrow"),FALSE,FALSE,0);
    app.case_title=label("COMMAND CENTER","mode-title");gtk_box_pack_start(GTK_BOX(hero),app.case_title,FALSE,FALSE,0);
    app.status=label("● AWAITING ORDERS","status");gtk_box_pack_start(GTK_BOX(hero),app.status,FALSE,FALSE,0);
    GtkWidget *row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_box_pack_start(GTK_BOX(main),row,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(row),label("LIVE CASE TRANSCRIPT","eyebrow"),TRUE,TRUE,0);
    gtk_box_pack_end(GTK_BOX(row),label("SECURE EVIDENCE STREAM  ●","muted"),FALSE,FALSE,0);
    app.scroll=gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(app.scroll),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    gtk_style_context_add_class(gtk_widget_get_style_context(app.scroll),"card");
    gtk_box_pack_start(GTK_BOX(main),app.scroll,TRUE,TRUE,0);
    app.transcript=gtk_text_view_new();gtk_text_view_set_editable(GTK_TEXT_VIEW(app.transcript),FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app.transcript),FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app.transcript),GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(app.transcript),20);gtk_text_view_set_right_margin(GTK_TEXT_VIEW(app.transcript),20);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(app.transcript),18);
    gtk_style_context_add_class(gtk_widget_get_style_context(app.transcript),"story");
    gtk_container_add(GTK_CONTAINER(app.scroll),app.transcript);
    app.buffer=gtk_text_view_get_buffer(GTK_TEXT_VIEW(app.transcript));
    gtk_text_buffer_create_tag(app.buffer,"accent","foreground","#64dfec","weight",PANGO_WEIGHT_BOLD,NULL);
    gtk_text_buffer_create_tag(app.buffer,"success","foreground","#6adfa6",NULL);
    gtk_text_buffer_create_tag(app.buffer,"error","foreground","#ff6b82",NULL);
    row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_box_pack_start(GTK_BOX(main),row,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(row),label("YOUR NEXT MOVE","eyebrow"),TRUE,TRUE,0);
    app.hint=label("CHOOSE A CASE TO BEGIN","status");gtk_box_pack_end(GTK_BOX(row),app.hint,FALSE,FALSE,0);
    app.choices=gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(app.choices),GTK_SELECTION_NONE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(app.choices),3);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(app.choices),1);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(app.choices),8);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(app.choices),8);
    /* Limit the choice area so a long menu cannot push entry/confirm below
       the bottom of the screen. Independent scroll keeps all actions usable. */
    GtkWidget *choice_scroll=gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(choice_scroll),
                                   GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(choice_scroll,-1,115);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(choice_scroll),90);
    gtk_style_context_add_class(gtk_widget_get_style_context(choice_scroll),"choices-scroll");
    gtk_container_add(GTK_CONTAINER(choice_scroll),app.choices);
    gtk_box_pack_start(GTK_BOX(main),choice_scroll,FALSE,FALSE,0);
    row=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);gtk_box_pack_start(GTK_BOX(main),row,FALSE,FALSE,0);
    app.entry=gtk_entry_new();gtk_entry_set_placeholder_text(GTK_ENTRY(app.entry),"Enter your choice...");
    gtk_box_pack_start(GTK_BOX(row),app.entry,TRUE,TRUE,0);
    g_signal_connect(app.entry,"activate",G_CALLBACK(on_confirm),NULL);
    app.send=gtk_button_new_with_label("CONFIRM  ↵");
    gtk_style_context_add_class(gtk_widget_get_style_context(app.send),"primary");
    g_signal_connect(app.send,"clicked",G_CALLBACK(on_confirm),NULL);
    gtk_box_pack_start(GTK_BOX(row),app.send,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(main),label("CASE ZERO V5   •   WINDOWS + LINUX   •   100% C SOURCE   •   PKT 12-HOUR CLOCK","muted"),FALSE,FALSE,0);
}
int main(int argc,char **argv){
    gtk_init(&argc,&argv);
    // Locate the sibling compiled engine, even when started from another directory.
    gchar *exe=g_canonicalize_filename(argv[0],g_get_current_dir());
    gchar *base=g_path_get_dirname(exe);
#ifdef G_OS_WIN32
    app.engine_path=g_build_filename(base,"case_zero_engine.exe",NULL);
#else
    app.engine_path=g_build_filename(base,"case_zero_engine",NULL);
#endif
    g_free(exe);g_free(base);
    app.started=g_get_monotonic_time();
    build_ui();tick(NULL);g_timeout_add_seconds(1,tick,NULL);
    gtk_widget_show_all(app.win);
    gtk_window_maximize(GTK_WINDOW(app.win)); /* respect OS taskbars/panels */
    launch_engine(0);
    gtk_main();
    stop_engine();reset_lines();g_free(app.engine_path);
    return 0;
}
