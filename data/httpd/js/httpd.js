let currentPath="/httpd",currentFile="";

/** Загрузка списка файлов и папок */
async function loadList(path=currentPath){
    currentPath=path;
    const r=await fetch("/api/list?path="+encodeURIComponent(path));
    const items=await r.json();
    document.getElementById("path").textContent=path;
    let html="";
    if(path!=="/httpd"){
        let parent=path.substring(0,path.lastIndexOf("/"));
        if(!parent)parent="/httpd";
        html+=`<div class="file-item"><button onclick="loadList('${parent}')">⬅ ..</button></div>`;
    }
    items.sort((a,b)=>a.dir&&!b.dir?-1:!a.dir&&b.dir?1:a.name.localeCompare(b.name));

for(const i of items){
    if(i.dir){
        html+=`
        <div class="file-item" onclick="loadList('${i.path}')">
            <div class="file-name">📁 ${i.name}</div>
            <div><button onclick="event.stopPropagation();loadList('${i.path}')">📂</button></div>
        </div>`;
    }else{
        html+=`
        <div class="file-item" onclick="viewFile('${i.path}')">
            <div class="file-name">📄 ${i.name}<span class="file-size">(${i.size})</span></div>
            <div>
                <button onclick="event.stopPropagation();viewFile('${i.path}')">👁</button>
                <button onclick="event.stopPropagation();downloadFile('${i.path}')">⬇</button>
                <button onclick="event.stopPropagation();deleteFile('${i.path}')">✖</button>
            </div>
        </div>`;
    }
}
//    for(const i of items){
//        if(i.dir){
//            html+=`<div class="file-item"><div class="file-name">📁 ${i.name}</div><div><button onclick="loadList('${i.path}')">📂</button></div></div>`;
//        }else{
//            html+=`<div class="file-item"><div class="file-name">📄 ${i.name}<span class="file-size">(${i.size})</span></div><div><button onclick="viewFile('${i.path}')">👁</button><button onclick="downloadFile('${i.path}')">⬇</button><button onclick="deleteFile('${i.path}')">✖</button></div></div>`;
//        }
//    }
    document.getElementById("fileList").innerHTML=html;
}

/** Открытие файла */
async function viewFile(file){
    currentFile=file;
    const r=await fetch("/api/file?name="+encodeURIComponent(file));
    document.getElementById("editor").value=await r.text();
}

/** Сохранение файла */
async function saveFile(){
    if(!currentFile){alert("Файл не выбран");return;}
    await fetch("/api/save",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"file="+encodeURIComponent(currentFile)+"&data="+encodeURIComponent(document.getElementById("editor").value)});
}

/** Создание файла */
async function createFile(){
    const name=prompt("Имя файла");
    if(!name)return;
    await fetch("/api/create",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded"},body:"file="+encodeURIComponent(currentPath+"/"+name)});
    loadList();
}

/** Удаление файла */
async function deleteFile(file){
    if(!confirm(file))return;
    await fetch("/api/delete?file="+encodeURIComponent(file));
    loadList();
}

/** Скачать файл */
function downloadFile(file){
    window.open("/download?file="+encodeURIComponent(file));
}

/** Загрузка файла */
async function uploadSelectedFile(){
    const input=document.getElementById("uploadFile");
    if(!input.files.length)return;

    const f=input.files[0];

//    if(!f.name.endsWith(".bin") && !f.name.endsWith(".txt") && !f.name.endsWith(".js")){
//        alert("Invalid file");
//        return;
//    }

    const form=new FormData();
//    form.append("file",f);
    form.append("path",currentPath);   // <<< ВАЖНО
    form.append("file", new File([f], currentPath+"/"+f.name));
    console.info(form);

    await fetch("/api/upload",{method:"POST",body:form});
    loadList();
}
/*
async function uploadSelectedFile() {

    const input =
        document.getElementById("uploadFile");

    if (!input.files.length) return;

    const form = new FormData();

    form.append("file", input.files[0]);

    await fetch("/api/upload", {
        method: "POST",
        body: form
    });

    loadList();
}
*/
/** Инициализация */
loadList();