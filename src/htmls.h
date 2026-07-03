
/*
 * This HTML code will be injected in /setup webpage using a <div></div> element as parent
 * The parent element will hhve the HTML id properties equal to 'raw-html-<id>'
 * where the id value will be equal to the id parameter passed to the function addHTML(html_code, id).
 */
static const char custom_html[] PROGMEM = R"EOF(
  <div id="iconcontent">
        <div id="form-con">
            <form>
                <label for="lametric-iconID">Icon ID</label><br>
                <input type="text" id="lametric-iconID" name="lametric-iconID"><br>
                <div class="button-row">
                    <input class="btn" type="button" value="Preview" onclick="createLametricLink()">
                    <input class="btn" type="button" value="Download" onclick="downloadLametricImage()">
                </div>
            </form>
        </div>
        <br>
        <br>
        <div id="icon-container">
        </div>
    </div>
)EOF";

static const char custom_css[] PROGMEM = R"EOF(
        .iconcontent {
            width: 50%;
            justify-content: center;
        }
        #form-con {
            width: 50%;
            margin: 0 auto;
            min-width: 200px;
        }
        .button-row input {
            width: 50%;
            margin: 0 5px;
        }
        .button-row {
            display: flex;
            justify-content: space-evenly;
            margin: 0 -5px;
            margin-top: 5px;
        }
        #icon-container {
            margin: 0 auto;
            max-width: 150px;
            max-height: 150px;
            width: 150px;
            background-color: black;
            height: 150px;
            margin: 0 auto;
        }
        #icon-container img {
            image-rendering: pixelated;
            max-width: 150px;
            max-height: 150px;
            width: 150px;
            background-color: black;
            height: 150px;
        }
	)EOF";

static const char custom_script[] PROGMEM = R"EOF(
function createLametricLink(){const e=document.getElementById("lametric-iconID").value,t=document.createElement("img");t.onerror=function(){openModalMessage("Error","<b>This ID doesnt exist</b>")},t.src="https://developer.lametric.com/content/apps/icon_thumbs/"+e;const n=document.getElementById("icon-container");n.innerHTML="",n.appendChild(t)}async function downloadLametricImage(){const e=document.getElementById("lametric-iconID").value;try{let n=await fetch("https://developer.lametric.com/content/apps/icon_thumbs/"+e),o=await n.blob();var t="";const c=n.headers.get("content-type");if("image/jpeg"===c||"image/png"===c){t=".jpg";let n=new Image,c=URL.createObjectURL(o);n.onload=function(){let o=document.createElement("canvas");o.width=n.width,o.height=n.height,o.getContext("2d").drawImage(n,0,0,n.width,n.height),o.toBlob((function(n){sendBlob(n,e,t)}),"image/jpeg",1),URL.revokeObjectURL(c)},n.src=c}else"image/gif"===n.headers.get("content-type")&&sendBlob(o,e,t=".gif")}catch(e){console.log("Error"),openModalMessage("Error","<b>This ID doesnt exist</b>")}}function sendBlob(e,t,n){const o=new FormData;o.append("image",e,"ICONS/"+t+n),fetch("/edit",{method:"POST",body:o,mode:"no-cors"}).then((e=>{e.ok&&openModalMessage("Finish","<b>Icon saved</b>")})).catch((e=>{console.log(e)}))}
)EOF";


static const char screen_html[] PROGMEM = R"EOF(
<!DOCTYPE html><html><script src="https://html2canvas.hertzen.com/dist/html2canvas.min.js"></script><head><title>LiveView</title><style>body,#a{margin:0;padding:0;display:flex;background:#000}body{justify-content:center;align-items:center;flex-direction:column;min-height:100vh}#a{position:relative;padding:60px}#a::before{content:"";position:absolute;top:0;left:0;right:0;bottom:0;background:url('https://raw.githubusercontent.com/Blueforcer/awtrix3/main/border.png') no-repeat center;background-size:cover;z-index:2}canvas{position:relative;max-width:100%;max-height:100%;background:#000;z-index:1}.b{display:flex;justify-content:center;align-items:center;gap:10px;margin-top:20px}.b button{width:150px;height:50px;color:#fff;background:#333}</style><script type="module">import{GIFEncoder,quantize,applyPalette}from'https://unpkg.com/gifenc@1.0.3';const c=document.getElementById('c'),d=c.getContext('2d'),w=640,h=320;c.width=w;c.height=h;let e,f=!1,g=performance.now();function j(){fetch("/api/screen").then(function(a){return a.json()}).then(function(a){d.clearRect(0,0,c.width,c.height);d.fillStyle="#000";for(let b=0;b<32;b++)for(let i=0;i<64;i++){const k=a[b*64+i],l=(k&0xff0000)>>16,m=(k&0x00ff00)>>8,n=k&0x0000ff;d.fillStyle=`rgb(${l},${m},${n})`;d.fillRect(i*10,b*10,8,8)}if(f){const o=performance.now(),p=Math.round((o-g));g=o;const q=d.getImageData(0,0,w,h).data,r="rgb444",s=quantize(q,256,{format:r}),t=applyPalette(q,s,r);e.writeFrame(t,w,h,{palette:s,delay:p})}j()})}document.addEventListener("DOMContentLoaded",function(){j();document.getElementById("h").addEventListener("click",function(){const a=document.createElement("a");a.href=c.toDataURL();a.download='awtrix.png';a.click()});document.getElementById("i").addEventListener("click",function(){const a=new XMLHttpRequest();a.open("POST","/api/nextapp",!0);a.send()});document.getElementById("j").addEventListener("click",function(){const a=new XMLHttpRequest();a.open("POST","/api/previousapp",!0);a.send()});document.getElementById("k").addEventListener("click",async function(){const a=this;if(f){e.finish();const b=e.bytesView();l(b,'awtrix.gif','image/gif');f=!1;a.textContent="Start GIF recording"}else{e=GIFEncoder();g=performance.now();f=!0;a.textContent="Stop GIF recording"}})});function l(b,a,c){const d=b instanceof Blob?b:new Blob([b],{type:c}),e=URL.createObjectURL(d),f=document.createElement("a");f.href=e;f.download=a;f.click()}</script></head><body><div id="a"><canvas id="c"></canvas></div><div class="b"><button id="j"><</button><button id="h">Download PNG</button><button id="k">Start GIF recording</button><button id="i">></button></div></body></html>
)EOF";


// backup_html + screenfull_html removed (unused).