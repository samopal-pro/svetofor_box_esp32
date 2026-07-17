/**
 * Рекурсивно загружает JSON-схему и подключает все include-файлы.
 * Поддерживает вложенные include.
 *
 * @param {string} url - URL JSON-схемы.
 * @returns {Promise<Object>} Объединенная схема.
 * @throws {Error} Если файл не найден, пустой или содержит некорректный JSON.
 */
async function loadSchema(url) {
    const response = await fetch(url);

    if (!response.ok)throw new Error(`Ошибка загрузки файла: ${url} (${response.status})`);

    const text = await response.text();

    if (!text.trim())throw new Error(`Пустой JSON-файл: ${url}`);

    let schema;

    try {
        schema = JSON.parse(text);
    }
    catch (err) {
        console.error(`Некорректный JSON: ${url}`,err);
    }

    if (!schema.form)return schema;

    const result = [];

    for (const item of schema.form) {
        if (item.include) {
            const included = await loadSchema(item.include);

            if (included.form)
                result.push(...included.form);
            else
                result.push(included);
        }
        else {
            result.push(item);
        }
    }

    schema.form = result;

    return schema;
}

// Глобальная переменная для отслеживания текущей секции конфигурации
let currentSection = null;

async function renderForm(schemaUrl, dataUrl, dataSection, containerId='formContainer') {
    let dataJson = {};
    let data = {};
    
    // Устанавливаем глобальную секцию
    currentSection = dataSection || null;
    console.log('Current config section set to:', currentSection);
    
    const schema = await loadSchema(schemaUrl);

    if( dataUrl !== '' ){
       dataJson = await fetch(dataUrl).then(r => r.json());
       if( dataSection !== '' )data = dataJson[dataSection];
    }
    const container = document.getElementById(containerId);
    const titleEl = document.getElementById('title');

    if (titleEl && schema.title) {
       const h1 = document.createElement('h1');
       h1.textContent = schema.title;
       titleEl.appendChild(h1);
    }

    new FormRenderer(schema, data, container ).render();
}

/*
* Рендерим всю форму по полям JSON-схемы 
*/
class FormRenderer {
    constructor(schema, data, container) {
        this.schema    = schema;
        this.data      = data;
        this.container = container;
    }

    render() {
        this.container.innerHTML = "";
        this.schema.form.forEach(fsCfg => {
            const fieldset = new Fieldset(this.container,fsCfg, this.data);
            fieldset.render();
        });
    }
}

/*
* Рендерим единичный набор полей из JSON схемы 
*/
class Fieldset {

    constructor(owner, config, values) {
        this.owner  = owner;
        this.config = config;
        this.values  = values;
    }

    render() {
// Моздаем секцию fieldset формы
        const fs = document.createElement("fieldset");

// Добавляему legend к fildset
        if (this.config.fieldset) {
            const legend = document.createElement("legend");
            legend.textContent = this.config.fieldset;
            fs.appendChild(legend);
        }

// Рендерим каждое поле 
        this.config.fields.forEach(fieldCfg => {
          if( fieldCfg.type === "table" ){
            const table = new Table(fs, fieldCfg, this.values );
            table.createElement();
          }
          else {
            const field = FieldFactory.create(fs, fieldCfg, this.values[fieldCfg.name] );
            field.createElement();
            field.createWrapper();
          }
        });
        this.owner.appendChild(fs);
    }
}

/*
* Селектор класса
*/
class FieldFactory {
    static create(owner, config, value, is_table=false) {
        switch (config.type) {
            case "label":     return new LabelField(owner, config, value, is_table); break;
            case "img":       return new ImgField(owner, config, value, is_table); break;
            case "text":      return new TextField(owner, config, value, is_table); break;
            case "password":  return new PasswordField(owner, config, value, is_table); break;
            case "hidden":    return new HiddenField(owner, config, value, is_table); break;
            case "checkbox":  return new CheckboxField(owner, config, value, is_table); break;
            case "radio":     return new RadioField(owner,config, value, is_table); break;
            case "number":    return new NumberField(owner, config, value, is_table); break;
            case "color":     return new ColorField(owner, config, value, is_table); break;
            case "color_box": return new ColorBoxField(owner, config, value, is_table); break;
            case "datalist":  return new DatalistField(owner, config, value, is_table); break;
            case "button":    return new ButtonField(owner, config, value, is_table); break;
            case "textarea":  return new TextareaField(owner, config, value, is_table);break;
            case "ip":        return new IpField(owner, config, value, is_table);break;
            case "select":    return new SelectField(owner, config, value, is_table); break;
            case "file":      return new FileField(owner, config, value, is_table); break;
            case "wifi":      return new WifiField(owner, config, value, is_table); break;            
//            case "table":     return new Table(owner, config, value); break;
            default:
                throw new Error(`Unknown field type: ${config.type}`);
        }
    }
}

/*
* Базовый класс для одного поля формы
*/
class FormField {
    constructor(owner, config, value = null, is_table=false) {
        this.owner   = owner;
        this.config  = config;
        this.value   = value;
        if(is_table)this.wrapper = document.createElement("span");
        else this.wrapper = document.createElement("p"); 
    }

    createElement() {
        console.error("createElement() must be implemented",this.config.type);
    }

    baseProperty(owner){
       if (this.config.placeholder)owner.placeholder = this.config.placeholder;
       if( this.config.width  )owner.style.width  = this.config.width;  
       if( this.config.height )owner.style.height = this.config.height;        
       if( this.config.class  )owner.classList.add(this.config.class);  
       if( this.config.request && this.config.cmd_is_null && owner.value === "" )this.createValueRequest(owner);
       if( this.config.request && this.config.cmd  )this.createValueRequest(owner);
       if( this.config.double )owner.dataset.double = this.config.double;
    }

    createLabel(){
      if (this.config.label) {
        const label = document.createElement("label");
        label.textContent = this.config.label;
//        label.style.width   = '100%';
//        label.style.display = 'flex';
//        label.style.justifyContent = 'space-between';
//        if( this.config.label_class )label.class = this.config.label_class;
        if( this.config.label_class )label.classList.add(this.config.label_class);
        this.wrapper.appendChild(label);
        return(label);
      }
      return this.wrapper;

    }

    createWrapper() { 
        this.owner.appendChild(this.wrapper);
    }
   
    async createValueRequest(el){
      try  {
        let cmd="";
        if(this.config.cmd_is_null)cmd = encodeURIComponent(this.config.cmd_is_null);
        else if(this.config.cmd)cmd = encodeURIComponent(this.config.cmd);
        else return;

        const body = new URLSearchParams();
        body.append("cmd", cmd);
        body.append("name", this.config.name || "");
        body.append("arg1", this.config.arg1 || "");
        body.append("arg2", this.config.arg2 || "");
        body.append("arg3", this.config.arg3 || "");

        const response = await fetch(this.config.request, { method: "POST", 
          headers: {"Content-Type":"application/x-www-form-urlencoded"}, 
          body: "cmd=" + cmd
        });
 //       const item = await response.json();
        if( this.config.type === "label")el.innerText = item.value;
        el.value = this.value;
      }
      catch(err)  { 
        console.error("Request error",this.config.type,this.config.name, this.value, err);    
       }
    }

}

/**
 * Невидимое поле для сохранения значения
 */
class HiddenField extends FormField {
    createElement() {
        const input = document.createElement("input");
        input.type = "hidden";
        input.name = this.config.name;
        input.value = this.value ?? "";
        this.wrapper.appendChild(input);
    }
}

/** 
* Поле типа TEXT
*/
class TextField extends FormField {
    createElement() {
        const input = document.createElement("input");
        this.baseProperty(input);
        input.type  = "text";
        input.name  = this.config.name;
        input.value = this.value ?? "";
        this.createLabel().appendChild(input);
    }
}

/** 
* Поле типа IP-адрес
*/
class IpField extends FormField {
    createElement() {
        const input = document.createElement("input");
        this.baseProperty(input);
        input.type  = "text";
        input.name  = this.config.name;
        input.value = this.value ?? "";
        input.pattern =  "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$";

//        if (!input.checkValidity()) {
//          input.style.borderColor = "red";
//        }


        input.addEventListener("blur", () => {
          if (!input.checkValidity()) {
            input.style.border = "2px solid red";
            input.reportValidity();
          } else {
            input.style.border = "";
          }
        });  
        this.createLabel().appendChild(input);
    }
}

/** 
 * Поле типа NUMBER
*/
class NumberField extends FormField {
    createElement() {
        const input = document.createElement("input");
        this.baseProperty(input);
        input.type  = "number";
        input.name  = this.config.name;
        input.min   = this.config.min ?? "";
        input.max   = this.config.max ?? "";
        input.value = this.value ?? "";
        this.createLabel().appendChild(input);
    }
}

/**
 * Поле COLOR
 */
class ColorField extends FormField {
    createElement() {
        const input = document.createElement("input");
        this.baseProperty(input);
        input.type = "color";
        input.name = this.config.name;
        input.value = this.value || "#000000";
         this.createLabel().appendChild(input);
    }
}

/**
 * Поле для ввода пароля
 */
class PasswordField extends FormField {
    createElement() {
        const wrapper = document.createElement("div");
        const input = document.createElement("input");
        this.baseProperty(input);
        input.type = "password";
        input.name  = this.config.name;
        input.value = this.value ?? "";
        const toggle = document.createElement("button");
        toggle.type = "button";
        toggle.textContent = "👁";
        toggle.style.background = 'transparent';
        toggle.style.border = 'none';
        toggle.style.padding = '0';
        toggle.style.marginLeft = '5px';
        toggle.style.cursor = 'pointer';
        toggle.style.fontSize = '20px';        
        toggle.addEventListener("click", () => {
            input.type = input.type === "password"
                ? "text"
                : "password";
        });
        wrapper.appendChild(input);
        wrapper.appendChild(toggle);
        this.createLabel().appendChild(wrapper);
    }
}

/**
 * Поле CheckBox
 */
class CheckboxField extends FormField {
    createElement() {
        const input = document.createElement("input");
        this.baseProperty(input);
        input.type = "checkbox";
        input.name = this.config.name;
        input.checked = !!this.value;
        this.createLabel().appendChild(input);
    }
}

/**
 * Поле Radio
 */
class RadioField extends FormField {
    createElement() {
        const input = document.createElement("input");
        this.baseProperty(input);
        input.type = "radio";
        input.name = this.config.name;
        input.value = this.config.value;
        if (String(this.value) === String(this.config.value)) {
            input.checked = true;
        }
        this.createLabel().appendChild(input);
    }
}

/**
 * Поле типа Label (просто текст)
 */
class LabelField extends FormField {
    createElement() {
        const field = document.createElement("span");
        this.baseProperty(field);
        field.innerText = this.config.value;
        if(this.config.name )field.id = this.config.name;
        this.wrapper.appendChild(field);
    }
}

/**
 * Поле типа IMG (вставить картинку)
 */
class ImgField extends FormField {
    createElement() {
        const field = document.createElement("img");
        this.baseProperty(field);
        field.src = this.config.value || this.config.src || "";
        this.wrapper.appendChild(field);
    }
}

/**
 * Поле - цветной квадратик, который меняет значения поля COLOR
 */
class ColorBoxField extends FormField {
    createElement() {
        const el = document.createElement("div");
        el.style.backgroundColor = this.value || "#000000";
        el.style.width = this.config.width || "30px";
        el.style.height = this.config.height || "20px";
        el.style.borderRadius = "2px";
        el.style.border = "1px solid #888";
        el.style.cursor = "pointer";
        el.style.display = "inline-block";

        const hidden = document.createElement("input");
        hidden.type = "hidden";
        hidden.name = this.config.name;
        hidden.value = this.value || "#000000";
        el.appendChild(hidden);
        el.addEventListener("click", () => {
        const target = document.querySelector(
          `input[name="${this.config.target}"]`
          );

          if (target) {
            target.value = el.style.backgroundColor;
            target.dispatchEvent(new Event("input"));
          }
        });
        this.wrapper.appendChild(el);
    }
}

/**
 * Кнопка
 */
class ButtonField extends FormField {
    async createRequest(el){
      try  {
        let cmd="";
        if(this.config.cmd_is_null)cmd = encodeURIComponent(this.config.cmd_is_null);
        else if(this.config.cmd)cmd = encodeURIComponent(this.config.cmd);
        else return;
        if( this.config.log )showMessage(this.config.log);
        if( this.config.reload )setTimeout(() => { location.reload();  }, this.config.reload);
//        if( this.config.wait ){
//            showMessage(this.config.wait);
//            startWaiting(1000);
//        }

        const body = new URLSearchParams();

        body.append("cmd", cmd);
        body.append("name", this.config.name || "");
        body.append("arg1", this.config.arg1 || "");
        body.append("arg2", this.config.arg2 || "");
        
        const response = await fetch(this.config.url, { method: "POST", 
          headers: {"Content-Type":"application/x-www-form-urlencoded"}, 
          body          
        });
//        const item = await response.json();
      }
      catch(err)  { 
        console.error("Request error",this.config.type,this.config.name, err);    
       }
    }

    async saveData(e, mode, name, label) {
       if (e) e.preventDefault();

       let inputs = document.querySelectorAll("input, select, textarea");
       let data = {};
//       data[name] = {};
    //   log(label);
       const form = new FormData();
       form.append("mode", mode);
       form.append("name", name)

       if( mode === "save" ){
          inputs.forEach(i => {
              this.saveItem(i,data,name);
              if( i.dataset.double )this.saveItem(i,data,i.dataset.double);
          });

          form.append("data", JSON.stringify(data));  
        }
        showMessage("Параметры сохранены","success");
        await fetch("/save", { method: "POST", body: form  });

    }

    saveItem(item,data,section){
        if(!item.name) return;
        if(!data[section])data[section] = {};
        if (item.type === "checkbox")data[section][item.name] = item.checked;
        else if (item.type === "radio") {
            if (item.checked) data[section][item.name] = item.value;
        } 
        else  data[section][item.name] = item.value;  
    }

    
    createElement() {
        const input = document.createElement("input");
        this.baseProperty(input);
        input.type = "button";
        input.value = this.config.label || "Button";

        input.addEventListener("click", async (e) => {

          if (this.config.mode === "url") { window.location.href = this.config.name; }
          else if (this.config.mode === "request") { this.createRequest(input); }

          else if (this.config.mode === "back") { window.history.back(); }
          else if (this.config.mode === "logout") { logout();  }
          else if (this.config.mode === "upload") { await uploadFile(e, this.config.file, "/upload");  }
          else if (this.config.mode === "update") { await uploadFile(e, this.config.file, "/update");  }
          else if (this.config.mode === "save"){ this.saveData(e, this.config.mode, this.config.name, this.config.label); }

        });
        this.wrapper.appendChild(input);
    }
}

/**
 * Поле типа SELECT
 */
class SelectField extends FormField {
    createOption(el){
      this.config.options?.forEach((opt, i) => {
        const option = document.createElement("option");
        option.value = opt.value;
        option.innerText = opt.name;
        if (String(this.value) === String(opt.value))option.selected = true;
        el.appendChild(option);
      });
    }

    async createOptionRequest(el){
      try  {
        const response = await fetch(this.config.request, { method: "POST", 
          headers: {"Content-Type":"application/x-www-form-urlencoded"}, 
          body: "cmd=" + encodeURIComponent(this.config.cmd)
        });
        const items = await response.json();
        items.forEach(item => {
          console.info(item.value, item.name); 
          if (String(this.value) !== String(item.value)){       
             const option = document.createElement("option");
             option.value = item.value;
             option.innerText = item.name;
//             if (String(this.value) === String(item.value))option.selected = true;
             el.appendChild(option);
          }
        });
      }
      catch(err)  { 
        console.error("datalist error", err);    
       }
    }

    createElement() {
        const el = document.createElement("select");
        this.baseProperty(el);
        el.name = this.config.name;
        if( (String(this.value) !== '' ) ){
          const option     = document.createElement("option");
          option.value     = String(this.value);
          option.innerText = String(this.value);
          option.selected  = true;
          el.appendChild(option);
        }
        if (this.config.options) this.createOption(el);
        else if(this.config.request) this.createOptionRequest(el);
          
        this.createLabel().appendChild(el);
    }
}

/**
 * Поле типа TEXT с DATALIST
 */
class DatalistField extends FormField {
    createElement() {
        const el = document.createElement("input");
        this.baseProperty(el);
        el.type = "text";
        el.name = this.config.name;
        el.id   = this.config.name;
        el.value = this.value || "";
        const listId = `LIST_${this.config.name}`;
        el.setAttribute("list", listId);
        const datalist = document.createElement("datalist");
        datalist.id = listId;
        if (this.config.options) {
          this.config.options?.forEach((opt, i) => {
            const option = document.createElement("option");
            option.value = opt;
            option.dataset.id = i;
//            option.label = opt;
            datalist.appendChild(option);
            });

            el.addEventListener("el", () => {
               const opt = [...datalist.options].find(o => o.value === el.value);
              if (opt) console.log("ID:", opt.dataset.id);
           });
        }
        else if(this.config.request){
           loadDataList(datalist, this.config.request, this.config.cmd);  
        }
        const label = this.createLabel();
        label.appendChild(el);
        label.appendChild(datalist);
    }
}

/**
 * Поле типа FILE
 */
class FileField extends FormField {
    createElement() {
        const el = document.createElement("input");
        this.baseProperty(el);
        el.type = "file";
        el.name = this.config.name;
        this.wrapper.appendChild(el);
    }
}

/**
 * Поле типа TEXTAREA
 */
class TextareaField extends FormField {
    createElement() {
        const textarea = document.createElement("textarea");
        this.baseProperty(textarea);

        textarea.name  = this.config.name;
        textarea.value = this.value ?? "";

        if (this.config.rows) textarea.rows = this.config.rows;
        if (this.config.cols) textarea.cols = this.config.cols;
//        if (this.config.placeholder)
//            textarea.placeholder = this.config.placeholder;

        this.createLabel().appendChild(textarea);
    }
}

/**
 * Создание таблицы с полями
 */
class Table  {
    constructor(owner, config, values) {
        this.owner   = owner;
        this.config  = config;
        this.values  = values;
 //       this.wrapper = document.createElement("p");
    }

 createElement() {
    const table = document.createElement("table");

    // Заголовок таблицы
    if (this.config.header) {
        const thead = document.createElement("thead");
        const trHead = document.createElement("tr");

        this.config.header.forEach(headCfg => {
            const th = document.createElement("th");
            th.textContent = headCfg.label || "";
            if (headCfg.width) th.style.width = headCfg.width;
            trHead.appendChild(th);
        });

        thead.appendChild(trHead);
        table.appendChild(thead);
    }

    // Тело таблицы
    const tbody = document.createElement("tbody");

    this.config.rows.forEach(rowCfg => {
        const tr = document.createElement("tr");

        rowCfg.forEach(cellCfg => {
            const td = document.createElement("td");

            if (cellCfg.td_width)
                td.style.width = cellCfg.td_width;

            const field = FieldFactory.create(td, cellCfg, this.values?.[cellCfg.name], true );

            field.createElement();
            field.createWrapper();

            tr.appendChild(td);
        });

        tbody.appendChild(tr);
    });

    table.appendChild(tbody);
    this.owner.appendChild(table);
}
}

async function loadDataList(datalist, url, cmd){
//    console.info(url,cmd);
    try  {
        const response = await fetch(url, {
            method: "POST",
            headers: {
                "Content-Type":
                    "application/x-www-form-urlencoded"
            },
            body: "cmd=" + encodeURIComponent(cmd)
        });
        const items = await response.json();
         datalist.innerHTML = "";
        items.forEach(item => {
            console.info(item.value, item.name);        
            const option = document.createElement("option");
            option.value = item.value;
            if (item.name)
                option.label = item.name;
            datalist.appendChild(option);
        });
    }
    catch(err)  {
        console.error("datalist error", err);
    }
}

    class WifiField extends FormField {
        createElement() {
            const container = document.createElement("div");
            container.className = "wifi-container";
            
            const input = document.createElement("input");
            this.baseProperty(input);
            input.type = "text";
            input.name = this.config.name;
            input.value = this.value ?? "";
            
            const button = document.createElement("button");

 
            button.type = "button";
            button.textContent = "▼";
//            button.value     = "▼";
//            button.className = "btn_scan";
            button.title = "Сканировать сети";
            button.style.background = 'transparent';
            button.style.border = 'none';
            button.style.padding = '0';
            button.style.marginLeft = '5px';
            button.style.cursor = 'pointer';
            button.style.fontSize = '20px';        
            
            const dropdown = document.createElement("div");
            dropdown.className = "wifi-dropdown";
            dropdown.style.display = "none";
            
            button.addEventListener("click", async (e) => {
                e.stopPropagation();
                if (dropdown.style.display === "block") {
                    dropdown.style.display = "none";
                    return;
                }
                dropdown.innerHTML = '<div class="wifi-scanning">Сканирование...</div>';
                dropdown.style.display = "block";
                try {
                    const networks = await this.scanNetworks();
                    this.buildDropdown(dropdown, networks, input);
                } catch (err) {
                    dropdown.innerHTML = '<div class="wifi-error">Ошибка сканирования</div>';
                    console.error("WiFi scan error:", err);
                }
            });
            
            document.addEventListener("click", () => {
                dropdown.style.display = "none";
            });
            
            container.addEventListener("click", (e) => {
                e.stopPropagation();
            });
            
            container.appendChild(input);
            container.appendChild(button);
            container.appendChild(dropdown);
            this.createLabel().appendChild(container);
        }
        
        async scanNetworks() {
            const response = await fetch(this.config.request, {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: "cmd=" + encodeURIComponent(this.config.cmd)
            });
            if (!response.ok) throw new Error("Scan failed");
            return await response.json();
        }
        
        buildDropdown(container, networks, input) {
            container.innerHTML = "";
            if (!networks.length) {
                container.innerHTML = '<div class="wifi-empty">Сети не найдены</div>';
                return;
            }
            networks.forEach(net => {
                const item = document.createElement("div");
                item.className = "wifi-item";
                item.textContent = net.ssid || net.name || net.value;
                if (net.signal) {
                    const signal = document.createElement("span");
                    signal.className = "wifi-signal";
 //                   signal.textContent = this.signalIcon(net.signal);
                    item.prepend(signal);
                }
                item.addEventListener("click", () => {
                    input.value =  net.value;
                    container.style.display = "none";
                });
                container.appendChild(item);
            });
        }
       
    }