/******************************************************************************
 Copyright (C) 2014 by John R. Bradley <jrb@turrettech.com>
 Copyright (C) 2018 by Hugh Bailey ("Jim") <jim@obsproject.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#pragma once

#include "cef-headers.hpp"
#include <string>
#include <fstream>

class BrowserSchemeHandlerFactory : public CefSchemeHandlerFactory {
public:
	virtual CefRefPtr<CefResourceHandler>
	Create(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame>,
	       const CefString &, CefRefPtr<CefRequest> request) override;

	IMPLEMENT_REFCOUNTING(BrowserSchemeHandlerFactory);
};

class BrowserSchemeHandler : public CefResourceHandler {
	std::string fileName;
	std::ifstream inputStream;
	bool isComplete = false;
	int64 length = 0;
	int64 remaining = 0;

public:
	virtual bool ProcessRequest(CefRefPtr<CefRequest> request,
				    CefRefPtr<CefCallback> callback) override;
	virtual void GetResponseHeaders(CefRefPtr<CefResponse> response,
					int64 &response_length,
					CefString &redirectUrl) override;
	virtual bool ReadResponse(void *data_out, int bytes_to_read,
				  int &bytes_read,
				  CefRefPtr<CefCallback> callback) override;
	virtual void Cancel() override;

	IMPLEMENT_REFCOUNTING(BrowserSchemeHandler);
};
