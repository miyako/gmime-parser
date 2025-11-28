![platform](https://img.shields.io/static/v1?label=platform&message=mac-intel%20|%20mac-arm%20|%20win-64&color=blue)
[![license](https://img.shields.io/github/license/miyako/gmime-parser)](LICENSE)
![downloads](https://img.shields.io/github/downloads/miyako/gmime-parser/total)

### Dependencies and Licensing

* the source code of this CLI tool is licensed under the MIT license.
* see [gmime](https://github.com/jstedfast/gmime/blob/master/LICENSE) for the licensing of **gmime** (LGPL).
 
# gmime-parser
CLI tool to extract text from EML

```
text extractor for eml documents

 -i path  : document to parse
 -o path  : text output (default=stdout)
 -        : use stdin for input
 -r       : raw text output (default=json)
```

## JSON

|Property|Level|Type|Description|
|-|-|-|-|
|document|0|||
|document.type|0|Text||
|document.meta|0|Object||
|document.meta.sender|0|Object||
|document.meta.from|0|Object||
|document.meta.to|0|Object||
|document.meta.cc|0|Object||
|document.meta.bcc|0|Object||
|document.meta.replyTo|0|Object||
|document.meta.subject|0|Text||
|document.pages|0|Array||
|document.pages[].paragraphs|1|Array||
|document.pages[].paragraphs[].text|2|Text||
