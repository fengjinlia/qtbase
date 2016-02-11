const char *templateQtLoader = R"STRING_DELIMITER(
// This script is generated by nacldeployqt as a part of the standard
// Qt deployment. It can be used either as-is or as a basis for a
// custom deployment solution. Note that some parts of this script
// (such as onMessage) is required by Qt.
//
// Usage:
//
// 1) Create and configure the Qt loader object:
//
//   var qtLoader = new QtLoader({
//       src : "myapp.nmf",
//       type : "application/pnacl",
//   });
//
//  Where 'myapp.nmf' is the nmf file for your application.
//
// 2) Create the Qt DOM element
//
//   var element = qtLoader.createElement();
//
// If you show this element it will display a standard Qt
// progress loader. (The alternative is to delay showing
// it until you get a 'load' event indicating load complete)
//
// 3) Connect load progress event listeners to the element
//
//
// 4) Start loading
//
//   qtLoader.load()
//
// This starts the download and pNaCl translation of the application
// binary.
//
// 5) Send end receive messages
//
//  element.postMessage(message) // ends up in pp::Instance::HandleMessage
//  element.addEventListener('message', function(message){ ... })
//
// Additional supported Qt constructor config keys
//   query           : Query string. Qt will parse this for environment variables.
//   isChromeApp     : enable Chrome Application package mode
//   loadText        : Loading placeholder text
//
function QtLoader(config) {
    var self = this;
    self.config = config;
    self.loadText = config.loadText !== undefined ? config.loadText : "Loading Qt:"
    self.type = config.type !== undefined ? config.type : "%APPTYPE%"
    self.loadTextElement = undefined;
    self.embed = undefined;
    self.listener = undefined;
    self.debug = false;

// Utility function for splitting Url query parameters
// ("?Foo=bar&Bar=baz") into key-value pars.
function decodeQuery(query) {
    if (query === undefined)
        return {}
    var vars = query.split('&');
    var keyValues = {}
    for (var i = 0; i < vars.length; i++) {
        // split key/value on the first '='
        var parts = vars[i].split(/=(.+)/);
        var key = decodeURIComponent(parts[0]);
        var value = decodeURIComponent(parts[1]);
        if (key && key.length > 0)
            keyValues[key] = value;
    }
    return keyValues;
}

function copyToClipboard(content)
{
    var copySource = document.createElement("div");
    copySource.contentEditable = true;
    var activeElement = document.activeElement.appendChild(copySource).parentNode;
    copySource.innerText = content;
    copySource.focus();
    document.execCommand("Copy", null, null);
    activeElement.removeChild(copySource);
}

function pasteFromClipboard()
{
    var pasteTarget = document.createElement("div");
    pasteTarget.contentEditable = true;
    var activeElement = document.activeElement.appendChild(pasteTarget).parentNode;
    pasteTarget.focus();
    document.execCommand("Paste", null, null);
    var content = pasteTarget.innerText;
    activeElement.removeChild(pasteTarget);
    return content;
}

// Qt message handler
function onMessage(messageEvent)
{
    // Expect messages to be in the form "tag:message",
    // and that the tag has a handler installed in the
    // qtMessageHandlers object.
    //
    // As a special case, messages with the "qtEval" tag
    // are evaluated with eval(). This allows Qt to inject
    // javaScript into the web page, for example to install
    // message handlers.

    if (this.qtMessageHandlers === undefined) {
        this.qtMessageHandlers = {}
        // Install message handlers needed by Qt:
        this.qtMessageHandlers["qtGetAppVersion"] = function(url) {
            embed.postMessage("qtGetAppVersion: " + navigator.appVersion);
        };
        this.qtMessageHandlers["qtOpenUrl"] = function(url) {
            window.open(url);
        };
        this.qtMessageHandlers["qtClipboardRequestCopy"] = function(content) {
            console.log("copy to clipbard " + content);
            copyToClipboard(content);
        };
        this.qtMessageHandlers["qtClipboardRequestPaste"] = function() {
            var content = pasteFromClipboard();
            console.log("paste from clipboard " + content);
            embed.postMessage("qtClipboardtPaste: " + content);
        };
    }

    // Give the message handlers access to the nacl module.
    var embed = self.embed

    var parts = messageEvent.data.split(/:(.+)/);
    var tag = parts[0];
    var message = parts[1];

    // Handle "qt" messages
    if (tag == "qtEval") {
        // "eval()" is not allowed for Chrome Apps
        if (self.config.isChromeApp !== undefined && !self.config.isChromeApp)
            eval(message)
    } else {
        if (this.qtMessageHandlers[tag] !== undefined)
            this.qtMessageHandlers[tag](message);
    }
}

function onProgress(event)
{
    if (self.debug)
        console.log("progress " + event.loaded  + " " + event.total)

    if (event.total == 0) {
        self.loadTextNode.innerHTML = self.loadText;
        return;
    }

    // calculate and pretty print load percentage
    var progress = 100 * event.loaded / event.total;
    var progressString = progress.toPrecision(2)
    if (progress == 100)
        progressString = 100;

    // update loader text
    self.loadTextNode.innerHTML = self.loadText + " " + progressString + "%"
}

function onLoad(event)
{
    if (self.debug)
        console.log("load: NaCl execution started.");

    // hide the loader and show the embed
    self.loadTextNode.style.display = "none"
    self.embed.style.position = "static"
    self.embed.style.visibility = "visible"
}

function loadScript(src, onload)
{
    var script = document.createElement('script')
    script.src = src
    script.onload = function () { onload() };
    document.head.appendChild(script);
}

// Create Qt container element, possibly re-using existingElement
function createElement(existingElement)
{
    // Create container div
    self.listener = existingElement || document.createElement("div");
    self.listener.setAttribute("class", "qt-container");

    // Add load and progress event listeners
    var useCapture = true;
    self.listener.addEventListener('message', onMessage, useCapture);
    self.listener.addEventListener('progress', onProgress, useCapture);
    self.listener.addEventListener('load', onLoad, useCapture);

    // Add loading text placeholder to container
    self.loadTextNode = document.createElement("span");
    self.loadTextNode.setAttribute("class", "qt-progress");
    self.listener.appendChild(self.loadTextNode);

    // Check for (P)NaCl support
    var pnaclSupported = navigator.mimeTypes['application/x-pnacl'] !== undefined;
    if (!pnaclSupported && self.type != "emscripten")
        self.loadTextNode.innerHTML = "Sorry! Google Chrome is required.";

    return self.listener;
}

function load()
{
    if (self.type == "emscripten")
        loadEmscripten()
    else
        loadNaCl();
}

function loadNaCl()
{
    // Create NaCl <embed> element.
    var embed = document.createElement("EMBED");
    self.embed = embed;
    embed.setAttribute("class", "qt-embed");
    embed.setAttribute("src", self.config.src);
    embed.setAttribute("type", self.type);

    // hide the nacl embed while loading. 'display = "none"' makes
    // NaCl stop loading; remove the element from the layout and
    // hide instead
    embed.style.position = "absolute"
    embed.style.visibility = "hidden"

    // Decode and set URL query string values as attributes on the embed tag.
    // This allows passing for example ?QSG_VISUALIZE=overdraw
    var queryHash = decodeQuery(self.config.query);
    for (var key in queryHash) {
        if (key !== undefined)
            embed.setAttribute(key, queryHash[key])
    }

    self.listener.appendChild(embed);
    self.listener.embed = embed;
}

function loadEmscripten()
{
    var width = self.listener.offsetWidth
    var height = self.listener.offsetHeight

    var embed = document.createElement("div");
    embed.setAttribute("class", "qt-embed");
    self.listener.appendChild(embed);
    self.listener.embed = embed;

    loadScript(config.src, function(){
        CreateInstance(width, height, embed);
        embed.finishLoading();
    })
}

function postMessage(message) {
    self.embed.postMessage(message)
}

// return object with public API
return {
    createElement : createElement,
    load : load,
    postMessage : postMessage
}

} // function QtLoader
)STRING_DELIMITER";
