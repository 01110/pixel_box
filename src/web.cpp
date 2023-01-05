#include "web.hpp"

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "ws2812b_8x8.hpp"

namespace pixelbox
{
  namespace web
  {
    AsyncWebServer server(80);    
    voidcb updated_cb = NULL;

    bool set_displayed_image(String name)
    {
      File di = LittleFS.open("/displayed_image", "w");
      if(!di) return false;
      if(di.write(name.c_str()) != name.length()) return false;
      di.close();
      if(updated_cb) updated_cb();
      return true;
    }

    bool get_displayed_image(String& filename)
    {
      File di = LittleFS.open("/displayed_image", "r");
      if(!di) return false;
      filename = di.readString();
      di.close();
      return true;
    }

    void select_next_image(String name)
    {
      Dir dir = LittleFS.openDir("/images");
      uint32_t file_count = 0;
      while (dir.next()) file_count++;

      if(file_count == 0 || file_count == 1)
      {
        set_displayed_image("");
        return;
      }

      dir.rewind();
      while (dir.next())
      {
        //if found the file, get the next
        if(dir.fileName() == name)
        {
          if(dir.next()) //the displayed was not the last file, we can select the next one
          {
            set_displayed_image(dir.fileName());
            return;
          }
          else //the displayed is the last file, we have to go back to the first one
          {
            dir.rewind();
            dir.next();
            set_displayed_image(dir.fileName());
            return;
          }
        }
      }
    }

    bool del_image(String name)
    {
      String displayed_image;
      if(!get_displayed_image(displayed_image)) return false;

      //if we want to delete the displayed image, we will set the next one as displayed
      if(name == displayed_image) select_next_image(displayed_image);

      return LittleFS.remove("/images/" + name);
    }

    void image_upload_req(AsyncWebServerRequest* request)
    {
      request->send(200);      
    }    

    void image_upload(AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final)
    {
      if(index == 0)
      {
        request->_tempFile = LittleFS.open("/images/" + filename, "w");
        if(!request->_tempFile)
        {
          request->send(500, "plain/text", "Failed to open file.");
          return;
        }
      }

      request->_tempFile.seek(index);
      size_t written = request->_tempFile.write(data, len);
      if(written != len)
      {
        request->send(500, "plain/text", "Write error.");
        return;
      }

      if (final)
      {
        request->_tempFile.close();

        if(!set_displayed_image(filename))
        {
          request->send(500, "plain/text", "Set error.");
          return;
        }

        request->send(200);
      }
    }

    void add_route(const char* route, routeCallbackFunction callback)
    {
      server.on(route, callback);
    }

    void add_updated_cb(voidcb callback)
    {
      updated_cb = callback;
    }

    String processor(const String& var)
    {
      fs::FSInfo info;
      LittleFS.info(info);
      if(var == "TOTAL_SIZE")
        return String(info.totalBytes / 1024);
      else if(var == "ALLOCATED_SIZE")
        return String(info.usedBytes / 1024);
      else if(var == "FREE_HEAP")
        return String(ESP.getFreeHeap());
      else if(var == "BRIGHTNESS")
      {
        File f = LittleFS.open("/brightness", "r");
        if(!f) return "";
        String ret = f.readString();
        f.close();
        return ret;
      }
      else if(var == "MAX_CURRENT")
      {
        File f = LittleFS.open("/max_current", "r");
        if(!f) return "";
        String ret = f.readString();
        f.close();
        return ret;
      }
      else
        return String();
    }

    void setup()
    {
      LittleFS.begin();

      server.on("/", HTTP_GET, [](AsyncWebServerRequest* request)
      {
        request->send(LittleFS, "/index.html", String(), false, processor);
      });
      server.on("/main.js", HTTP_GET, [](AsyncWebServerRequest* request)
      {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/main.js.gz", "text/javascript", false, nullptr);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
      });
      server.on("/mvp.css", HTTP_GET, [](AsyncWebServerRequest* request)
      {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/mvp.css.gz", "text/css", false, nullptr);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
      });
      server.on("/displayed_image", HTTP_GET, [](AsyncWebServerRequest* request)
      {
        //read displayed image filename and send it
        String filename;
        if(!get_displayed_image(filename))
        {
          request->send(500, "plain/text", "Can't read displayed image file.");
          return;
        }

        String content_type = "unkown";
        if(filename.endsWith(".png")) content_type = "image/png";
        else if(filename.endsWith(".gif")) content_type = "image/gif";

        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/images/" + filename, content_type, false, nullptr);
        response->addHeader("Cache-Control", "no-cache");
        request->send(response);
      });
      server.on("/displayed_image", HTTP_POST, [](AsyncWebServerRequest* request)
      {
        String filename = request->arg("displayed_image");
        if(!set_displayed_image(filename))
        {
          request->send(500, "plain/text", "Failed to set displayed_image");
          return;
        }
        request->send(200);
      });
      server.on("/image", HTTP_POST, image_upload_req, image_upload);
      server.on("/image", HTTP_DELETE, [](AsyncWebServerRequest* request)
      {
        String filename = request->arg("image");
        if(!del_image(filename))
        {
          request->send(500, "plain/text", "Failed to delete image.");
          return;
        }
        request->send(200);
      });
      server.on("/images", HTTP_GET, [](AsyncWebServerRequest* request)
      {
        String output;
        output += "{\"images\": [";

        Dir dir = LittleFS.openDir("/images");
        bool first = true;
        while (dir.next())
        {
          if(!first) output += ",";
          else first = false;
          output += "\"" + dir.fileName() + "\"";
        }

        output += "]}";
        request->send(200, "text/json", output);
      });
      server.on("/fs_status", HTTP_GET, [](AsyncWebServerRequest* request)
      {
        String output;
        output += "{\"total_size\":" + processor("TOTAL_SIZE") + ", \"allocated_size\":" + processor("ALLOCATED_SIZE") + ", \"free_heap\":" + processor("FREE_HEAP") + "}";
        request->send(200, "text/json", output);
      });
      server.on("/set_brightness", HTTP_POST, [](AsyncWebServerRequest* request)
      {
        File br = LittleFS.open("/brightness", "w");
        if(!br)
        {
          request->send(500, "plain/text", "Failed to save brightness.");
          return;
        }
        if(br.write(request->arg("brightness").c_str()) != request->arg("brightness").length())
        {
          request->send(500, "plain/text", "Failed to save brightness.");
          return;
        }
        br.close();
        pixelbox::ws2812b_8x8::set_brightness_percent(request->arg("brightness").toInt());
        request->send(200);
      });
      server.on("/set_max_current", HTTP_POST, [](AsyncWebServerRequest* request)
      {
        File br = LittleFS.open("/max_current", "w");
        if(!br)
        {
          request->send(500, "plain/text", "Failed to save max current.");
          return;
        }
        if(br.write(request->arg("max_current").c_str()) != request->arg("max_current").length())
        {
          request->send(500, "plain/text", "Failed to save max current.");
          return;
        }
        br.close();
        pixelbox::ws2812b_8x8::set_max_current(request->arg("max_current").toInt());
        request->send(200);
      });
      server.begin();
    }
  }
}