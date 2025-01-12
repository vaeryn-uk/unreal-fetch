// Copyright (c) 2020 Arunoda Susiripala. All Rights Reserved.


#include "FetchRequest.h"

UFetchRequest::UFetchRequest()
{

}

void UFetchRequest::Process(FString Url, FFetchOptions Options)
{
	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(HttpMethodToString(Options.Method));
	Request->SetHeader(TEXT("User-Agent"), TEXT("unreal-fetch"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("text/plain"));

	for (int lc = 0; lc < Options.Headers.Num(); lc++) {
		FFetchHeader Header = Options.Headers[lc];
		Request->SetHeader(Header.Key, Header.Value);
	}

	if (Options.Method != Get)
	{
		Request->SetContentAsString(Options.Body);
	}

	Request->OnProcessRequestComplete().BindUObject(this, &UFetchRequest::OnResponse);
	Request->ProcessRequest();
	bStarted = true;
}

void UFetchRequest::OnResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
{
	bFinished = true;
	bSuccessful = bSuccess;

	if (!bSuccess)
	{
		if (Response.IsValid())
		{
			OnErrorDelegate.ExecuteIfBound("Request Failed with an error: " + Response->GetContentAsString());
			return;
		}


		OnErrorDelegate.ExecuteIfBound("Request failed without making a request");
		return;
	}

	FetchResponse = UFetchResponse::Get(Response);

	OnTextDelegate.ExecuteIfBound(FetchResponse->ResponseText, FetchResponse);

	// JSON parsing
	TSharedPtr<FJsonValue> ParsedJSON;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FetchResponse->ResponseText);

	if (OnJsonDelegate.IsBound()) {
		// Special case: if the server sends back no content, we still consider this "ok" json.
		if (FetchResponse->StatusCode == 204 && FetchResponse->ResponseText.Len() == 0)
		{
			OnJsonDelegate.ExecuteIfBound(nullptr, FetchResponse);
			return;
		}

		if (FJsonSerializer::Deserialize(Reader, ParsedJSON))
		{
			OnJsonDelegate.ExecuteIfBound(USimpleJsonValue::Get(ParsedJSON), FetchResponse);
		}
		else {
			OnErrorDelegate.ExecuteIfBound("Invalid JSON as the response");
		}
	}
}

FString UFetchRequest::HttpMethodToString(FFetchOptionsMethod Method)
{
	switch (Method)
	{
	case Get:
		return "GET";

	case Post:
		return "POST";

	case Put:
		return "PUT";

	case Delete:
		return "DELETE";
	}

	return "";
}

UFetchRequest* UFetchRequest::OnText(FFetchTextResponseDelegate Event)
{
	OnTextDelegate = Event;
	return this;
}

UFetchRequest* UFetchRequest::OnError(FFetchErrorDelegate Event)
{
	OnErrorDelegate = Event;
	return this;
}

UFetchRequest* UFetchRequest::OnJson(FFetchJsonResponseDelegate Event)
{
	OnJsonDelegate = Event;
	return this;
}
