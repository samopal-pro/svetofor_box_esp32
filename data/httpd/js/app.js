function log(msg, type = "info") {
  console.log(msg);

  if (msg === "reload") {
     location.reload();
     return;
  }
  
  const div = document.getElementById("log");
  if (!div) return;
  const p = document.createElement("div");
  p.innerText = msg;
  if (type === "error") {
    p.style.color = "#880000";
  }
  else if (type === "ok") {
    p.style.color = "#00ff66";
  }
  else {
    p.style.color = "#000088";
  }
  div.innerHTML = "";
  div.appendChild(p);
//  div.scrollTop = div.scrollHeight;
}

function watchChannels() {
  setInterval(async () => {
   try {
    const response = await fetch('/channels');
    const channels = await response.json();

    channels.forEach(item => {
      const el = document.getElementById(item.NAME);

      if (el) {
        el.textContent = item.VALUE;
        el.style.color = item.COLOR;
      }
    });

  } catch (error) {
    console.error('Ошибка загрузки /channels:', error);
  }
  }, 500);  
}

function watchStatus() {
  setInterval(async () => {
    try {

      const r = await fetch("/status");
      const txt = await r.text();
      const div = document.getElementById("log");
      if (!div || div.lastText === txt) return;
      div.lastText = txt;
      let type = "info";
      let msg = txt;
      if (txt.includes("|")) {
        [type, msg] = txt.split("|", 2);
      }
      log(msg, type);
    } catch(e) {
      console.log(e);
    }
  }, 500);
}


async function loadJSON(url) {
    try {
        const response = await fetch(url);

        if (!response.ok) {
            return {};
        }

        return await response.json();
    }
    catch (err) {
//        console.warn(`Не удалось загрузить ${url}:`, err);
        return {};
    }
}

async function loadJSON1(url) {
  return await fetch(url).then(r => r.json());
}

//async function loadAll() {
//  formSchema = await loadJSON("/form1.json");
//  formData = await loadJSON("/data1.json");
//}

// MENU
async function renderMenu(menu_file) {
  let menuSchema = await loadJSON(menu_file);
  const path = window.location.pathname;
  const div = document.getElementById("menu");
  let m = document.createElement("div");
  m.className = "menu";
  menuSchema.menu.forEach(item => {
    let a = document.createElement("a");
    a.href = item.url;
    a.innerText = item.name;
    let name = item.play_name || "";
    let arg1 = item.play_arg1 || "";
    if( name != "" ){
       a.addEventListener("click", () => {
           playMP3(name,arg1,"");
       });
    }   
//    console.log(path,item.name);
    if( path === item.url )a.classList.add("active");
    m.appendChild(a);
  });
  div.appendChild(m);
}

async function uploadFile(e, inputName, url) {
  if (e) e.preventDefault();
  
  const input = document.querySelector(`input[name="${inputName}"]`);
  if (!input || !input.files.length) {
    log("Файл не выбран","error");
    return;
  }
  log("Загрузка файла ...");
  const form = new FormData();
  form.append("file", input.files[0]);

  const res = await fetch(url, {
    method: "POST",
    body: form
  });

  if (res.ok) log("Файл загружен","ok");
  else log("Ошибка загрузки","error");
}

async function loadText(uri,id) {
  try {
    const response = await fetch(uri);
    if (!response.ok)throw new Error('Ошибка загрузки header: ' + response.status);
    const html = await response.text();
    document.getElementById(id).innerHTML = html;
  } 
  catch (error) {
    console.error(error);     
  }
}

function watchDistance(uri,tm=1000) {
  setInterval(async () => {
    try {
      const r = await fetch(uri);
      const txt = await r.text();
      document.getElementById('dist').innerHTML = txt;
    } catch(e) {
      console.log(e);
    }
  }, tm);
}
/*
async function saveData(e, mode, name, label) {
  if (e) e.preventDefault();

  let inputs = document.querySelectorAll("input, select");
  let data = {};
  log(label);
  const form = new FormData();
  form.append("mode", mode);
  form.append("name", name)

  if( mode === "save" ){
    inputs.forEach(i => {
      console.log(i.name, i.type, i.value);
      if (!i.name) return;
    
      if (i.type === "checkbox")
        data[i.name] = i.checked;
      else if (i.type === "radio") {
        if (i.checked) data[i.name] = i.value;
      } else
      data[i.name] = i.value;
    });

    form.append("data", JSON.stringify(data));  
  }
  await fetch("/save", {
    method: "POST",
    body: form
  });

//  log("Сохранено","ok");
}
*/

async function logout(){
    try    {
        await fetch('/auth', {
            method: 'POST',
            headers: {
                'Content-Type':
                    'application/x-www-form-urlencoded'
            },
            body: 'cmd=LOGOUT'
        });
    }
    finally    {
        location.href = '/login.html';
    }
}

async function playMP3(_name,_arg1,_arg2="",_cmd="play"){
    try    {
       const body = new URLSearchParams();
        body.append("cmd", _cmd);
        body.append("name", _name || "");
        body.append("arg1", _arg1 || "");
        body.append("arg2", _arg2 || "");


        await fetch('/cmd', {
            method: 'POST',
            headers: {
                'Content-Type':
                    'application/x-www-form-urlencoded'
            },
            body
        });
    }
    finally    {
//        location.href = '/login.html';
    }
}

let waitTimer = null;

/*
 * Запуск опроса сервера
 */
function startWaiting(interval = 1000) {
    if (waitTimer) return;
    waitTimer = setInterval(checkCommand, interval);
}

/*
 * Остановка опроса сервера
 */
function stopWaiting() {
    if (!waitTimer) return;
    clearInterval(waitTimer);
    waitTimer = null;
}

/*
 * Проверка наличия команды от ESP32
 */
async function checkCommand() {
    try {
        const response = await fetch('/response', { cache: 'no-cache' });

        if (response.status === 204) return;
        if (response.status === 200)stopWaiting();
        if (!response.ok)   return;

        const { msg, cmd } = await response.json();

        if (msg) showMessage(msg);

        switch (cmd) {
            case 'reload':
                location.reload();
                break;
            case 'reload5':
                setTimeout(() => { location.reload();  }, 5000);
                break;

            case 'home':
                location.href = '/';
                break;

            case 'settings':
                location.href = '/settings';
                break;

            default:
                if (cmd)
                    console.warn(`Неизвестная команда: ${cmd}`);
        }
    }
    catch (err) {
        console.error('Ошибка связи:', err);
    }
}

/*
 * Показать временное сообщение
 * type: info, success, error
 */
function showMessage(text, type = 'info') {

    const icons = {
        info: 'ℹ️',
        success: '✅',
        error: '❌'
    };

    const box = document.createElement('div');

    box.innerHTML = `
        <span style="font-size:22px">${icons[type] || icons.info}</span>
        <span>${text}</span>
    `;

    box.style.position = 'fixed';
    box.style.left = '50%';
    box.style.top = '20%';
    box.style.transform = 'translate(-50%, -50%)';
    box.style.display = 'flex';
    box.style.gap = '10px';
    box.style.alignItems = 'center';
    box.style.padding = '15px 25px';
    box.style.background = type === 'error' ? '#b00020' :
                          type === 'success' ? '#087f23' : '#333';
    box.style.color = '#fff';
    box.style.borderRadius = '8px';
    box.style.zIndex = '9999';
    box.style.cursor = 'pointer';
    box.style.boxShadow = '0 2px 10px #000';

    document.body.appendChild(box);

    const close = () => box.remove();

    box.onclick = close;

    setTimeout(close, 4000);
}
