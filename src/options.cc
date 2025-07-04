/* GST123 - GStreamer based command line media player
 * Copyright (C) 2006-2010 Stefan Westerfeld
 * Copyright (C) 2010 أحمد المحمودي (Ahmed El-Mahmoudy)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <assert.h>
#include <gst/gst.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <stdlib.h>

#include "config.h"
#include "options.h"
#include "gtkinterface.h"
#include "configfile.h"
#include "visualization.h"

using std::string;

Options *Options::instance = NULL;

Options::Options ()
{
  assert (!instance);
  instance = this; // singleton

  program_name = "gst123";
  repeat  = FALSE;
  shuffle = FALSE;
  verbose = FALSE;
  novideo = FALSE;
  notags  = FALSE;
  quiet   = FALSE;
  fullscreen = FALSE;
  uris = NULL;
  audio_output = NULL;
  video_size = NULL;
  print_visualization_list = FALSE;
  visualization = NULL;
  skip = 0;
  initial_volume = -1; // don't touch volume setting when started

  string default_audio_output = ConfigFile::the().audio_output();
  if (default_audio_output != "")
    audio_output = g_strdup (default_audio_output.c_str());    // leak copy

  string default_visualization = ConfigFile::the().visualization();
  if (default_visualization != "")
    visualization = g_strdup (default_visualization.c_str()); // leak copy

  string default_video_size = ConfigFile::the().video_size();
  if (default_video_size != "")
    video_size = g_strdup (default_video_size.c_str());
}

void
Options::parse (int argc, char **argv)
{
  gboolean random = FALSE; // --random is equivalent to --shuffle --repeat
  GOptionContext *context = g_option_context_new ("<URI>... - Play video and audio clips");
  const GOptionEntry all_options[] = {
    {"list", '@', G_OPTION_FLAG_FILENAME, G_OPTION_ARG_CALLBACK,
      gpointer (static_cast<GOptionArgFunc> (Options::add_playlist)),
      "Read playlist of files and URIs from <filename>", "<filename>"},
    {"version", '\0', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
      gpointer (static_cast<GOptionArgFunc> (Options::print_version)), "Print version", NULL },
    {"full-version", '\0', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
      gpointer (static_cast<GOptionArgFunc> (Options::print_full_version)), "Print full version", NULL },
    {"verbose", '\0', 0, G_OPTION_ARG_NONE, &instance->verbose,
      "Print GStreamer pipeline used to play files", NULL},
    {"repeat", 'r', 0, G_OPTION_ARG_NONE, &instance->repeat,
      "Repeat playlist forever", NULL},
    {"shuffle", 'z', 0, G_OPTION_ARG_NONE, &instance->shuffle,
      "Shuffle playlist before playing", NULL},
    {"random",  'Z', 0, G_OPTION_ARG_NONE, &random,
      "Play files in random order forever", NULL},
    {"novideo", 'x', 0, G_OPTION_ARG_NONE, &instance->novideo,
      "Do not play the video stream", NULL},
    {"video-size", 'X', 0, G_OPTION_ARG_STRING, &instance->video_size,
      "Set video/plugin size (default 640x400)", "<WxH>"},
    {"fullscreen", 'f', 0, G_OPTION_ARG_NONE, &instance->fullscreen,
      "Use fullscreen video output", NULL},
    {"audio-output", 'a', 0, G_OPTION_ARG_STRING, &instance->audio_output,
      "Set audio output driver and device", "<driver>[=<dev>]"},
    {"visualization", 'v', 0, G_OPTION_ARG_STRING, &instance->visualization,
      "Set visualization plugin to use for audio playback", "<plugin_name>"},
    {"visualization-list", 'V', 0, G_OPTION_ARG_NONE, &instance->print_visualization_list,
      "Show available visualization plugins", NULL },
    {"skip", 'k', 0, G_OPTION_ARG_DOUBLE, &instance->skip,
      "Skip the first 'n' seconds", "<n>"},
    { "volume", '\0', 0, G_OPTION_ARG_DOUBLE, &instance->initial_volume,
      "Set initial replay volume (in percent)", "<volume>" },
    {"subtitle", 's', 0, G_OPTION_ARG_STRING, &instance->subtitle,
      "Set subtitle file to use for video playback", "<subtitle_file>"},
    {"notags", 't', 0, G_OPTION_ARG_NONE, &instance->notags,
      "Don't display tags", NULL},
    {"quiet", 'q', 0, G_OPTION_ARG_NONE, &instance->quiet,
      "Don't display any messages", NULL},
    {G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &instance->uris, "Movies to play", NULL},
    {NULL} /* end the list */
  };
  g_option_context_add_main_entries (context, all_options, NULL);
  g_option_context_add_group (context, gst_init_get_option_group());

  if (GtkInterface::have_x11_display())
    g_option_context_add_group (context, gtk_get_option_group (TRUE));

  GError *error = NULL;
  bool option_parse_ret = g_option_context_parse (context, &argc, &argv, &error);
  usage = g_option_context_get_help (context, TRUE, NULL);
  if (!option_parse_ret)
    {
      g_print ("%s\n%s", error->message, usage.c_str());
      g_error_free (error);
      g_option_context_free (context);
      exit (1);
    }
  g_option_context_free (context);

  if (random)
    {
      shuffle = TRUE;
      repeat = TRUE;
    }
}

gboolean
Options::print_version (const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
  printf ("%s %s\n", instance->program_name.c_str(), VERSION);
  exit (0);
}

gboolean
Options::print_full_version (const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
  printf ("%-10s %s\n", (instance->program_name + ":").c_str(), VERSION);
  printf ("%-10s %d.%d.%d-%d\n", "GStreamer:", GST_VERSION_MAJOR, GST_VERSION_MINOR, GST_VERSION_MICRO, GST_VERSION_NANO);
  printf ("%-10s %u.%u.%u\n", "GTK+:", gtk_major_version, gtk_minor_version, gtk_micro_version);
  printf ("%-10s %u.%u.%u\n", "GLib:", glib_major_version, glib_minor_version, glib_micro_version);

  exit (0);
}

gboolean
Options::add_playlist (const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
  instance->playlists.push_back (value);

  return TRUE;
}

const Options&
Options::the()
{
  assert (instance);

  return *instance;
}
