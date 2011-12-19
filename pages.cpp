// base_objects.cpp - Source to manage GUI base objects

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include <string>

extern "C" {
#include "../common.h"
#include "../minui/minui.h"
#include "../recovery_ui.h"
}

#include "rapidxml.hpp"
#include "objects.hpp"

extern int gGuiRunning;

std::map<std::string, PageSet*> PageManager::mPageSets;
PageSet* PageManager::mCurrentSet;


// Helper routine to convert a string to a color declaration
int ConvertStrToColor(std::string str, COLOR* color)
{
    // Set the default, solid black
    memset(color, 0, sizeof(COLOR));
    color->alpha = 255;

    // Translate variables
    DataManager::GetValue(str, str);
    
    // Look for some defaults
    if (str == "black")         return 0;
    else if (str == "white")    { color->red = color->green = color->blue = 255; return 0; }
    else if (str == "red")      { color->red = 255; return 0; }
    else if (str == "green")    { color->green = 255; return 0; }
    else if (str == "blue")     { color->blue = 255; return 0; }

    // At this point, we require an RGB(A) color
    if (str[0] != '#')          return -1;
    str.erase(0, 1);

    int result = strtol(str.c_str(), NULL, 16);
    if (str.size() > 6)
    {
        // We have alpha channel
        color->alpha = result & 0x000000FF;
        result = result >> 8;
    }
    color->red = (result >> 16) & 0x000000FF;
    color->green = (result >> 8) & 0x000000FF;
    color->blue = result & 0x000000FF;
    return 0;
}

// Helper APIs
bool LoadPlacement(xml_node<>* node, int* x, int* y, int* w /* = NULL */, int* h /* = NULL */, RenderObject::Placement* placement /* = NULL */)
{
    if (!node)      return false;

    std::string value;
    if (node->first_attribute("x"))
    {
        value = node->first_attribute("x")->value();
        DataManager::GetValue(value, value);
        *x = atol(value.c_str());
    }

    if (node->first_attribute("y"))
    {
        value = node->first_attribute("y")->value();
        DataManager::GetValue(value, value);
        *y = atol(value.c_str());
    }

    if (w && node->first_attribute("w"))
    {
        value = node->first_attribute("w")->value();
        DataManager::GetValue(value, value);
        *w = atol(value.c_str());
    }

    if (h && node->first_attribute("h"))
    {
        value = node->first_attribute("h")->value();
        DataManager::GetValue(value, value);
        *h = atol(value.c_str());
    }

    if (placement && node->first_attribute("placement"))
    {
        value = node->first_attribute("placement")->value();
        DataManager::GetValue(value, value);
        *placement = (RenderObject::Placement) atol(value.c_str());
    }

    return true;
}

int ActionObject::SetActionPos(int x, int y, int w, int h)
{
    if (x < 0 || y < 0)                                     return -1;

    mActionX = x; 
    mActionY = y; 
    if (w || h)
    {
        mActionW = w;
        mActionH = h;
    }
    return 0;
}

Page::Page(xml_node<>* page, xml_node<>* templates /* = NULL */)
{
    mTouchStart = NULL;

    // We can memset the whole structure, because the alpha channel is ignored
    memset(&mBackground, 0, sizeof(COLOR));

    // With NULL, we make a console-only display
    if (!page)
    {
        mName = "console";

        GUIConsole* element = new GUIConsole(NULL);
        mRenders.push_back(element);
        mActions.push_back(element);
        return;
    }

    if (page->first_attribute("name"))
        mName = page->first_attribute("name")->value();
    else
    {
        LOGE("No page name attribute found!\n");
        return;
    }

    LOGI("Loading page %s\n", mName.c_str());

    // This is a recursive routine for template handling
    ProcessNode(page, templates);

    return;
}

bool Page::ProcessNode(xml_node<>* page, xml_node<>* templates /* = NULL */, int depth /* = 0 */)
{
    if (depth == 10)
    {
        LOGE("Page processing depth has exceeded 10. Failing out. This is likely a recursive template.\n");
        return false;
    }

    // Let's retrieve the background value, if any
    xml_node<>* bg = page->first_node("background");
    if (bg)
    {
        xml_attribute<>* attr = bg->first_attribute("color");
        if (attr)
        {
            std::string color = attr->value();
            ConvertStrToColor(color, &mBackground);
        }
    }

    xml_node<>* child;
    child = page->first_node("object");
    while (child)
    {
        if (!child->first_attribute("type"))
            break;

        std::string type = child->first_attribute("type")->value();

        if (type == "text")
        {
            GUIText* element = new GUIText(child);
            mRenders.push_back(element);
            mActions.push_back(element);
        }
        else if (type == "image")
        {
            GUIImage* element = new GUIImage(child);
            mRenders.push_back(element);
        }
        else if (type == "fill")
        {
            GUIFill* element = new GUIFill(child);
            mRenders.push_back(element);
        }
        else if (type == "action")
        {
            GUIAction* element = new GUIAction(child);
            mActions.push_back(element);
        }
        else if (type == "console")
        {
            GUIConsole* element = new GUIConsole(child);
            mRenders.push_back(element);
            mActions.push_back(element);
        }
        else if (type == "button")
        {
            GUIButton* element = new GUIButton(child);
            mRenders.push_back(element);
            mActions.push_back(element);
        }
        else if (type == "checkbox")
        {
            GUICheckbox* element = new GUICheckbox(child);
            mRenders.push_back(element);
            mActions.push_back(element);
        }
        else if (type == "fileselector")
        {
            GUIFileSelector* element = new GUIFileSelector(child);
            mRenders.push_back(element);
            mActions.push_back(element);
        }
        else if (type == "animation")
        {
            GUIAnimation* element = new GUIAnimation(child);
            mRenders.push_back(element);
        }
        else if (type == "progressbar")
        {
            GUIProgressBar* element = new GUIProgressBar(child);
            mRenders.push_back(element);
            mActions.push_back(element);
        }
        else if (type == "template")
        {
            if (!templates || !child->first_attribute("name"))
            {
                LOGE("Invalid template request.\n");
            }
            else
            {
                std::string name = child->first_attribute("name")->value();

                // We need to find the correct template
                xml_node<>* node;
                node = templates->first_node("template");

                while (node)
                {
                    if (!node->first_attribute("name"))
                        continue;

                    if (name == node->first_attribute("name")->value())
                    {
                        if (!ProcessNode(node, templates, depth + 1))
                            return false;
                        else
                            break;
                    }
                    node = node->next_sibling("template");
                }
            }
        }
        else
        {
            LOGE("Unknown object type.\n");
        }
        child = child->next_sibling("object");
    }
    return true;
}

int Page::Render(void)
{
    // Render background - Ignore alpha, it must be solid
    gr_color(mBackground.red, mBackground.green, mBackground.blue, 255);
    gr_fill(0, 0, gr_fb_width(), gr_fb_height());

    // Render remaining objects
    std::vector<RenderObject*>::iterator iter;
    for (iter = mRenders.begin(); iter != mRenders.end(); iter++)
    {
        if ((*iter)->Render())
            LOGE("A render request has failed.\n");
    }
    return 0;
}

int Page::Update(void)
{
    int retCode = 0;

    std::vector<RenderObject*>::iterator iter;
    for (iter = mRenders.begin(); iter != mRenders.end(); iter++)
    {
        int ret = (*iter)->Update();
        if (ret < 0)
            LOGE("An update request has failed.\n");
        else if (ret > retCode)
            retCode = ret;
    }

    return retCode;
}

int Page::NotifyTouch(TOUCH_STATE state, int x, int y)
{
    // By default, return 1 to ignore further touches if nobody is listening
    int ret = 1;

    // Don't try to handle a lack of handlers
    if (mActions.size() == 0)   return ret;

    // We record mTouchStart so we can pass all the touch stream to the same handler
    if (state == TOUCH_START)
    {
        std::vector<ActionObject*>::reverse_iterator iter;
        // We work backwards, from top-most element to bottom-most element
        for (iter = mActions.rbegin(); iter != mActions.rend(); iter++)
        {
            if ((*iter)->IsInRegion(x, y))
            {
                mTouchStart = (*iter);
                ret = mTouchStart->NotifyTouch(state, x, y);
                if (ret >= 0)   break;
                mTouchStart = NULL;
            }
        }
    }
    else if (state == TOUCH_RELEASE && mTouchStart != NULL)
    {
        ret = mTouchStart->NotifyTouch(state, x, y);
        mTouchStart = NULL;
    }
    else if (state == TOUCH_DRAG && mTouchStart != NULL)
    {
        ret = mTouchStart->NotifyTouch(state, x, y);
    }
    return ret;
}

int Page::NotifyKey(int key)
{
    std::vector<ActionObject*>::reverse_iterator iter;

    // Don't try to handle a lack of handlers
    if (mActions.size() == 0)   return 1;

    // We work backwards, from top-most element to bottom-most element
    for (iter = mActions.rbegin(); iter != mActions.rend(); iter++)
    {
        int ret = (*iter)->NotifyKey(key);
        if (ret == 0)
            return 0;
        else if (ret < 0)
            LOGE("An action handler has returned an error");
    }
    return 1;
}

void Page::SetPageFocus(int inFocus)
{
    // Render remaining objects
    std::vector<RenderObject*>::iterator iter;
    for (iter = mRenders.begin(); iter != mRenders.end(); iter++)
    {
        (*iter)->SetPageFocus(inFocus);
    }
    return;
}

int Page::NotifyVarChange(std::string varName, std::string value)
{
    std::vector<ActionObject*>::iterator iter;

    // Don't try to handle a lack of handlers
    if (mActions.size() == 0)   return 1;

    for (iter = mActions.begin(); iter != mActions.end(); ++iter)
    {
        if ((*iter)->NotifyVarChange(varName, value))
            LOGE("An action handler errored on NotifyVarChange.\n");
    }
    return 0;
}

PageSet::PageSet(char* xmlFile)
{
    mResources = NULL;
    mCurrentPage = NULL;

    mXmlFile = xmlFile;
    if (xmlFile)
        mDoc.parse<0>(mXmlFile);
    else
        mCurrentPage = new Page(NULL);
}

PageSet::~PageSet()
{
    delete mResources;
    free(mXmlFile);
}

int PageSet::Load(ZipArchive* package)
{
    xml_node<>* parent;
    xml_node<>* child;
    xml_node<>* templates;
 
    parent = mDoc.first_node("recovery");
    if (!parent)
        parent = mDoc.first_node("install");

    // Now, let's parse the XML
    LOGI("Loading resources...\n");
    child = parent->first_node("resources");
    if (child)
        mResources = new ResourceManager(child, package);

    LOGI("Loading variables...\n");
    child = parent->first_node("variables");
    if (child)
        LoadVariables(child);

    LOGI("Loading pages...\n");
    // This may be NULL if no templates are present
    templates = parent->first_node("templates");

    child = parent->first_node("pages");
    if (!child)
        return -1;

    return LoadPages(child, templates);
}

int PageSet::SetPage(std::string page)
{
    Page* tmp = FindPage(page);
    if (tmp)
    {
        if (mCurrentPage)   mCurrentPage->SetPageFocus(0);
        mCurrentPage = tmp;
        mCurrentPage->SetPageFocus(1);
        mCurrentPage->NotifyVarChange("", "");
        return 0;
    }
    else
    {
        LOGE("Unable to locate page (%s)\n", page.c_str());
    }
    return -1;
}

Resource* PageSet::FindResource(std::string name)
{
    return mResources ? mResources->FindResource(name) : NULL;
}

Page* PageSet::FindPage(std::string name)
{
    std::vector<Page*>::iterator iter;

    for (iter = mPages.begin(); iter != mPages.end(); iter++)
    {
        if (name == (*iter)->GetName())
            return (*iter);
    }
    return NULL;
}

int PageSet::LoadVariables(xml_node<>* vars)
{
    xml_node<>* child;

    child = vars->first_node("variable");
    while (child)
    {
        if (!child->first_attribute("name"))
            break;
        if (!child->first_attribute("value"))
            break;

        DataManager::SetValue(child->first_attribute("name")->value(), child->first_attribute("value")->value());
        child = child->next_sibling("variable");
    }
    return 0;
}

int PageSet::LoadPages(xml_node<>* pages, xml_node<>* templates /* = NULL */)
{
    xml_node<>* child;

    if (!pages)    return -1;

    child = pages->first_node("page");
    while (child != NULL)
    {
        Page* page = new Page(child, templates);
        if (page->GetName().empty())
        {
            LOGE("Unable to process load page\n");
            delete page;
        }
        else
        {
            mPages.push_back(page);
        }
        child = child->next_sibling("page");
    }
    if (mPages.size() > 0)
        return 0;
    return -1;
}

int PageSet::IsCurrentPage(Page* page)
{
    return ((mCurrentPage && mCurrentPage == page) ? 1 : 0);
}

int PageSet::Render(void)
{
    return (mCurrentPage ? mCurrentPage->Render() : -1);
}

int PageSet::Update(void)
{
    return (mCurrentPage ? mCurrentPage->Update() : -1);
}

int PageSet::NotifyTouch(TOUCH_STATE state, int x, int y)
{
    return (mCurrentPage ? mCurrentPage->NotifyTouch(state, x, y) : -1);
}

int PageSet::NotifyKey(int key)
{
    return (mCurrentPage ? mCurrentPage->NotifyKey(key) : -1);
}

int PageSet::NotifyVarChange(std::string varName, std::string value)
{
    return (mCurrentPage ? mCurrentPage->NotifyVarChange(varName, value) : -1);
}

int PageManager::LoadPackage(std::string name, std::string package)
{
    int fd;
    ZipArchive zip, *pZip = NULL;
    long len;
    char* xmlFile = NULL;
    PageSet* pageSet = NULL;
    int ret;

    // Open the XML file
    LOGI("Loading package: %s (%s)\n", name.c_str(), package.c_str());
    if (mzOpenZipArchive(package.c_str(), &zip))
    {
        // We can try to load the XML directly...
        struct stat st;
        if(stat(package.c_str(),&st) != 0)
            return -1;

        len = st.st_size;
        xmlFile = (char*) malloc(len + 1);
        if (!xmlFile)       return -1;

        fd = open(package.c_str(), O_RDONLY);
        if (fd == -1)       goto error;

        read(fd, xmlFile, len);
        close(fd);
    }
    else
    {
        pZip = &zip;
        const ZipEntry* ui_xml = mzFindZipEntry(&zip, "ui.xml");
        if (ui_xml == NULL)
        {
            LOGE("Unable to locate ui.xml in zip file\n");
            goto error;
        }
    
        // Allocate the buffer for the file
        len = mzGetZipEntryUncompLen(ui_xml);
        xmlFile = (char*) malloc(len + 1);
        if (!xmlFile)        goto error;
    
        if (!mzExtractZipEntryToBuffer(&zip, ui_xml, (unsigned char*) xmlFile))
        {
            LOGE("Unable to extract ui.xml\n");
            goto error;
        }
    }

    // NULL-terminate the string
    xmlFile[len] = 0x00;

    // Before loading, mCurrentSet must be the loading package so we can find resources
    pageSet = mCurrentSet;
    mCurrentSet = new PageSet(xmlFile);

    ret = mCurrentSet->Load(pZip);
    if (ret == 0)
    {
        mCurrentSet->SetPage("main");
        mPageSets.insert(std::pair<std::string, PageSet*>(name, mCurrentSet));
    }
    else
    {
        LOGE("Package %s failed to load.\n", name.c_str());
    }
    mCurrentSet = pageSet;

    if (pZip)   mzCloseZipArchive(pZip);
    return ret;

error:
    LOGE("An internal error has occurred.\n");
    if (pZip)       mzCloseZipArchive(pZip);
    if (xmlFile)    free(xmlFile);
    return -1;
}

PageSet* PageManager::FindPackage(std::string name)
{
    std::map<std::string, PageSet*>::iterator iter;

    iter = mPageSets.find(name);
    if (iter != mPageSets.end())
    {
        return (*iter).second;
    }
    LOGE("Unable to locate package %s\n", name.c_str());
    return NULL;
}

PageSet* PageManager::SelectPackage(std::string name)
{
    LOGI("Switching packages (%s)\n", name.c_str());
    PageSet* tmp;

    tmp = FindPackage(name);
    if (tmp)
        mCurrentSet = tmp;
    else
        LOGE("Unable to find package.\n");

    return mCurrentSet;
}

int PageManager::ReloadPackage(std::string name, std::string package)
{
    std::map<std::string, PageSet*>::iterator iter;

    iter = mPageSets.find(name);
    if (iter == mPageSets.end())
        return -1;

    PageSet* set = (*iter).second;
    mPageSets.erase(iter);

    if (LoadPackage(name, package) != 0)
    {
        LOGE("Failed to load package.\n");
        mPageSets.insert(std::pair<std::string, PageSet*>(name, set));
        return -1;
    }
    if (mCurrentSet == set)     SelectPackage(name);
    delete set;
    return 0;
}

void PageManager::ReleasePackage(std::string name)
{
    std::map<std::string, PageSet*>::iterator iter;

    iter = mPageSets.find(name);
    if (iter == mPageSets.end())
        return;

    PageSet* set = (*iter).second;
    mPageSets.erase(iter);
    delete set;
    return;
}

int PageManager::ChangePage(std::string name)
{
    int ret = (mCurrentSet ? mCurrentSet->SetPage(name) : -1);
    DataManager::SetValue("tw_operation_state", 0);
    return ret;
}

Resource* PageManager::FindResource(std::string name)
{
    return (mCurrentSet ? mCurrentSet->FindResource(name) : NULL);
}

Resource* PageManager::FindResource(std::string package, std::string name)
{
    PageSet* tmp;

    tmp = FindPackage(name);
    return (tmp ? tmp->FindResource(name) : NULL);
}

int PageManager::SwitchToConsole(void)
{
    PageSet* console = new PageSet(NULL);

    mCurrentSet = console;
    return 0;
}

int PageManager::IsCurrentPage(Page* page)
{
    return (mCurrentSet ? mCurrentSet->IsCurrentPage(page) : 0);
}

int PageManager::Render(void)
{
    return (mCurrentSet ? mCurrentSet->Render() : -1);
}

int PageManager::Update(void)
{
    return (mCurrentSet ? mCurrentSet->Update() : -1);
}

int PageManager::NotifyTouch(TOUCH_STATE state, int x, int y)
{
    return (mCurrentSet ? mCurrentSet->NotifyTouch(state, x, y) : -1);
}

int PageManager::NotifyKey(int key)
{
    return (mCurrentSet ? mCurrentSet->NotifyKey(key) : -1);
}

int PageManager::NotifyVarChange(std::string varName, std::string value)
{
    return (mCurrentSet ? mCurrentSet->NotifyVarChange(varName, value) : -1);
}

extern "C" void gui_notifyVarChange(const char *name, const char* value)
{
    if (!gGuiRunning)   return;

    PageManager::NotifyVarChange(name, value);
}

