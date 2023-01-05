const original_size = 8;

function upload_img(event)
{
  event.preventDefault();
  const file = document.querySelector('input[type="file"]').files[0];

  var form_data = new FormData();
  form_data.append('file', file)

  fetch("image", {
    method: "POST",
    body: form_data
  }).then(function(resp)
  {
    refresh_image();
    refresh_image_list();
    refresh_fs_status();
  });
}

function delete_image(name)
{
  fetch('image', {
    method: 'DELETE',
    headers:{
      'Content-Type': 'application/x-www-form-urlencoded'
    },    
    body: new URLSearchParams({'image': name})
  }).then(function(resp)
  {
    refresh_image_list();
    refresh_image();
    refresh_fs_status();
  });
}

function refresh_image_list()
{
  fetch("images")
    .then(response => response.json())
    .then(data => {
      let table = document.getElementById("image_list");
      table.innerHTML = ''; //deleting the existing list

      if(data.images.length == 0)
      {
        let div = document.createElement('p');
        div.appendChild(document.createTextNode("No images."));
        table.appendChild(div);
        return;
      }

      for (let i = 0; i < data.images.length; i++)
      {
        let image_name = data.images[i];
        let tr = document.createElement('tr');

        let name = document.createElement('td');
        name.appendChild(document.createTextNode(image_name));

        let sel_d = document.createElement('td');
        let sel_btn = document.createElement('a');
        sel_btn.appendChild(document.createTextNode("select"));
        sel_btn.onclick = () => {set_displayed_image(image_name)};
        sel_d.appendChild(sel_btn);

        let del_d = document.createElement('td');
        let del_btn = document.createElement('a');
        del_btn.appendChild(document.createTextNode("delete"));
        del_btn.onclick = () => {delete_image(image_name)};
        del_d.appendChild(del_btn);

        tr.appendChild(name);
        tr.appendChild(sel_d);
        tr.appendChild(del_d);

        table.appendChild(tr);
      }
    });
}

function refresh_image()
{
  const ctx = document.getElementById("displayed_image").getContext("2d");

  // Load image
  const image = new Image();
  image.onload = () => {
    // Draw the image into the canvas
    ctx.drawImage(image, 0, 0);
  };
  let ts = new Date().getTime();
  image.src = "displayed_image?t=" + ts; //force reload image
}

function refresh_fs_status()
{
  fetch("fs_status")
    .then(response => response.json())
    .then(data => 
    {
      let total = document.getElementById("total_size");
      total.innerText = data.total_size + " KBytes";

      let allocated = document.getElementById("allocated_size");
      allocated.innerText = data.allocated_size + " KBytes";

      let free_heap = document.getElementById("free_heap");
      free_heap.innerText = data.free_heap + " Bytes";
    });
}

function set_displayed_image(name)
{
  fetch('displayed_image', {
    method: 'POST',
    headers:{
      'Content-Type': 'application/x-www-form-urlencoded'
    },    
    body: new URLSearchParams({'displayed_image': name})
  }).then(function(resp)
  {
    refresh_image();
  });
}

function brightness_changed()
{
  let percent = document.getElementById("brightness_range").value;
  document.getElementById("brightness_label").innerText = "Brightness: " + percent + "%";
  fetch('set_brightness', {
    method: 'POST',
    headers:{
      'Content-Type': 'application/x-www-form-urlencoded'
    },    
    body: new URLSearchParams({'brightness': percent})
  });
}

function max_current_changed()
{
  let current = document.getElementById("max_current_range").value;
  document.getElementById("max_current_label").innerText = "Max current: " + current + " mA";
  fetch('set_max_current', {
    method: 'POST',
    headers:{
      'Content-Type': 'application/x-www-form-urlencoded'
    },    
    body: new URLSearchParams({'max_current': current})
  });
}

document.getElementById("upload_form").onsubmit = upload_img;
refresh_image();
refresh_image_list();
setInterval(() => {
  refresh_fs_status();
  refresh_image();
  refresh_image_list();
}, 10000);