#include "browser-panel-internal.hpp"
#include "browser-panel-client.hpp"
#include "cef-headers.hpp"
#include "browser-app.hpp"

#include <QWindow>

#ifdef USE_QT_LOOP
#include <QApplication>
#include <QEventLoop>
#include <QThread>
#endif

#include <obs-module.h>
#include <util/threading.h>
#include <util/base.h>
#include <thread>

extern bool QueueCEFTask(std::function<void()> task);
extern "C" void obs_browser_initialize(void);
extern os_event_t *cef_started_event;

std::mutex popup_whitelist_mutex;
std::vector<PopupWhitelistInfo> popup_whitelist;
std::vector<PopupWhitelistInfo> forced_popups;

/* ------------------------------------------------------------------------- */

CefRefPtr<CefCookieManager> QCefRequestContextHandler::GetCookieManager()
{
	return cm;
}

class CookieCheck : public CefCookieVisitor {
public:
	QCefCookieManager::cookie_exists_cb callback;
	std::string target;
	bool cookie_found = false;

	inline CookieCheck(QCefCookieManager::cookie_exists_cb callback_,
			   const std::string target_)
		: callback(callback_), target(target_)
	{
	}

	virtual ~CookieCheck() { callback(cookie_found); }

	virtual bool Visit(const CefCookie &cookie, int, int, bool &) override
	{
		CefString cef_name = cookie.name.str;
		std::string name = cef_name;

		if (name == target) {
			cookie_found = true;
			return false;
		}
		return true;
	}

	IMPLEMENT_REFCOUNTING(CookieCheck);
};

struct QCefCookieManagerInternal : QCefCookieManager {
	CefRefPtr<CefCookieManager> cm;
	CefRefPtr<CefRequestContextHandler> rch;
	CefRefPtr<CefRequestContext> rc;

	QCefCookieManagerInternal(const std::string &storage_path,
				  bool persist_session_cookies)
	{
		if (os_event_try(cef_started_event) != 0)
			throw "Browser thread not initialized";

		BPtr<char> path = obs_module_config_path(storage_path.c_str());

		cm = CefCookieManager::CreateManager(
			path.Get(), persist_session_cookies, nullptr);
		if (!cm)
			throw "Failed to create cookie manager";

		rch = new QCefRequestContextHandler(cm);

		rc = CefRequestContext::CreateContext(
			CefRequestContext::GetGlobalContext(), rch);
	}

	virtual bool DeleteCookies(const std::string &url,
				   const std::string &name) override
	{
		return cm->DeleteCookies(url, name, nullptr);
	}

	virtual bool SetStoragePath(const std::string &storage_path,
				    bool persist_session_cookies) override
	{
		BPtr<char> path = obs_module_config_path(storage_path.c_str());

		return cm->SetStoragePath(path.Get(), persist_session_cookies,
					  nullptr);
	}

	virtual bool FlushStore() override { return cm->FlushStore(nullptr); }

	virtual void CheckForCookie(const std::string &site,
				    const std::string &cookie,
				    cookie_exists_cb callback) override
	{
		CefRefPtr<CookieCheck> c = new CookieCheck(callback, cookie);
		cm->VisitUrlCookies(site, false, c);
	}
};

/* ------------------------------------------------------------------------- */

QCefWidgetInternal::QCefWidgetInternal(QWidget *parent, const std::string &url_,
				       CefRefPtr<CefRequestContext> rqc_)
	: QCefWidget(parent), url(url_), rqc(rqc_)
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);

	setFocusPolicy(Qt::ClickFocus);
}

QCefWidgetInternal::~QCefWidgetInternal()
{
	CefRefPtr<CefBrowser> browser = cefBrowser;
	if (!!browser) {
		auto destroyBrowser = [](CefRefPtr<CefBrowser> cefBrowser) {
			if (!cefBrowser) {
				return;
			}
			CefRefPtr<CefClient> client =
				cefBrowser->GetHost()->GetClient();
			QCefBrowserClient *bc =
				reinterpret_cast<QCefBrowserClient *>(
					client.get());

			cefBrowser->GetHost()->WasHidden(true);
			cefBrowser->GetHost()->CloseBrowser(true);

			bc->widget = nullptr;
		};

#ifdef USE_QT_LOOP
		QueueBrowserTask(cefBrowser, destroyBrowser);
#else
		QueueCEFTask([=]() { destroyBrowser(browser); });
#endif
		cefBrowser = nullptr;
	}
}

void QCefWidgetInternal::Init()
{
	QSize size = this->size() * devicePixelRatio();
	WId id = winId();

	bool success = QueueCEFTask([this, size, id]() {
		CefWindowInfo windowInfo;

		/* Make sure Init isn't called more than once. */
		if (cefBrowser)
			return;

#ifdef _WIN32
		RECT rc = {0, 0, size.width(), size.height()};
		windowInfo.SetAsChild((HWND)id, rc);
#elif __APPLE__
		windowInfo.SetAsChild((CefWindowHandle)id, 0, 0, size.width(),
				      size.height());
#endif

		CefRefPtr<QCefBrowserClient> browserClient =
			new QCefBrowserClient(this, script);

		CefBrowserSettings cefBrowserSettings;
		cefBrowser = CefBrowserHost::CreateBrowserSync(
			windowInfo, browserClient, url, cefBrowserSettings,
			rqc);
#ifdef _WIN32
		Resize();
#endif
	});

	if (success)
		timer.stop();
}

void QCefWidgetInternal::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	Resize();
}

void QCefWidgetInternal::Resize()
{
#ifdef _WIN32
	QSize size = this->size() * devicePixelRatio();

	QueueCEFTask([this, size]() {
		if (!cefBrowser)
			return;
		HWND hwnd = cefBrowser->GetHost()->GetWindowHandle();
		SetWindowPos(hwnd, nullptr, 0, 0, size.width(), size.height(),
			     SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
		SendMessage(hwnd, WM_SIZE, 0,
			    MAKELPARAM(size.width(), size.height()));
	});
#endif
}

void QCefWidgetInternal::showEvent(QShowEvent *event)
{
	QWidget::showEvent(event);

	if (!cefBrowser) {
		obs_browser_initialize();
		connect(&timer, SIGNAL(timeout()), this, SLOT(Init()));
		timer.start(500);
		Init();
	}
}

QPaintEngine *QCefWidgetInternal::paintEngine() const
{
	return nullptr;
}

void QCefWidgetInternal::setURL(const std::string &url)
{
	if (cefBrowser) {
		cefBrowser->GetMainFrame()->LoadURL(url);
	}
}

void QCefWidgetInternal::setStartupScript(const std::string &script_)
{
	script = script_;
}

/* ------------------------------------------------------------------------- */

struct QCefInternal : QCef {
	virtual bool init_browser(void) override;
	virtual bool initialized(void) override;
	virtual bool wait_for_browser_init(void) override;

	virtual QCefWidget *
	create_widget(QWidget *parent, const std::string &url,
		      QCefCookieManager *cookie_manager) override;

	virtual QCefCookieManager *
	create_cookie_manager(const std::string &storage_path,
			      bool persist_session_cookies) override;

	virtual BPtr<char>
	get_cookie_path(const std::string &storage_path) override;

	virtual void add_popup_whitelist_url(const std::string &url,
					     QObject *obj) override;
	virtual void add_force_popup_url(const std::string &url,
					 QObject *obj) override;
};

bool QCefInternal::init_browser(void)
{
	if (os_event_try(cef_started_event) == 0)
		return true;

	obs_browser_initialize();
	return false;
}

bool QCefInternal::initialized(void)
{
	return os_event_try(cef_started_event) == 0;
}

bool QCefInternal::wait_for_browser_init(void)
{
	return os_event_wait(cef_started_event) == 0;
}

QCefWidget *QCefInternal::create_widget(QWidget *parent, const std::string &url,
					QCefCookieManager *cm)
{
	QCefCookieManagerInternal *cmi =
		reinterpret_cast<QCefCookieManagerInternal *>(cm);

	return new QCefWidgetInternal(parent, url, cmi ? cmi->rc : nullptr);
}

QCefCookieManager *
QCefInternal::create_cookie_manager(const std::string &storage_path,
				    bool persist_session_cookies)
{
	try {
		return new QCefCookieManagerInternal(storage_path,
						     persist_session_cookies);
	} catch (const char *error) {
		blog(LOG_ERROR, "Failed to create cookie manager: %s", error);
		return nullptr;
	}
}

BPtr<char> QCefInternal::get_cookie_path(const std::string &storage_path)
{
	return obs_module_config_path(storage_path.c_str());
}

void QCefInternal::add_popup_whitelist_url(const std::string &url, QObject *obj)
{
	std::lock_guard<std::mutex> lock(popup_whitelist_mutex);
	popup_whitelist.emplace_back(url, obj);
}

void QCefInternal::add_force_popup_url(const std::string &url, QObject *obj)
{
	std::lock_guard<std::mutex> lock(popup_whitelist_mutex);
	forced_popups.emplace_back(url, obj);
}

extern "C" EXPORT QCef *obs_browser_create_qcef(void)
{
	return new QCefInternal();
}
