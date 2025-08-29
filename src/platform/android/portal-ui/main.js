(function() {
  let token = null;
  let rows = [];
  let selectedRows = new Set();
  let sortField = 'fileName';
  let sortOrder = 'asc';
  let editingCell = null;

  renderContent();
  setupEventListeners();

  function renderContent() {
    const mainContent = document.getElementById('main-content');
    if (token) {
      renderFileList(mainContent);
      setupFileListEventListeners();
    } else {
      renderTokenInput(mainContent);
    }
  }

  function renderTokenInput(container) {
    container.innerHTML = `
      <div class="login-container">
        <div class="login-form">
          <div class="form-group">
            <label for="token-input">Enter your access token</label>
            <input type="text" id="token-input" autofocus>
            <div class="helper-text" id="helper-text">
              Enter the access token displayed on the SmallBASIC [About] screen.
            </div>
          </div>
          <button class="btn" id="submit-btn">Submit</button>
        </div>
      </div>
    `;

    const tokenInput = document.getElementById('token-input');
    const submitBtn = document.getElementById('submit-btn');

    tokenInput.addEventListener('keypress', (e) => {
      if (e.key === 'Enter') {
        handleLogin();
      }
    });

    submitBtn.addEventListener('click', () => {
      handleLogin();
    });
  }

  function renderFileList(container) {
    container.innerHTML = `
      <div class="toolbar">
        <button class="btn btn-small btn-icon" id="upload-btn">
          📤 UPLOAD
        </button>
        <button class="btn btn-small btn-icon" id="download-btn" disabled>
          📥 DOWNLOAD
        </button>
        <button class="btn btn-small btn-icon" id="delete-btn" disabled>
          🗑️ DELETE
        </button>
        <div id="selection-info" style="margin-left: auto; color: #666;"></div>
      </div>
      <div class="table-container">
        <table>
          <thead>
            <tr>
              <th>
                <input type="checkbox" class="checkbox" id="select-all">
              </th>
              <th data-field="fileName" class="sortable" style="cursor: pointer;">
                Name <span class="sort-indicator active">↑</span>
              </th>
              <th data-field="size" class="sortable" style="cursor: pointer;">
                Size <span class="sort-indicator">↕</span>
              </th>
              <th data-field="date" class="sortable" style="cursor: pointer;">
                Modified <span class="sort-indicator">↕</span>
              </th>
            </tr>
          </thead>
          <tbody id="file-table-body">
            <!-- Rows will be inserted here -->
          </tbody>
        </table>
      </div>
    `;

    renderTableRows();
    updateToolbarState();
  }

  function renderTableRows() {
    const tbody = document.getElementById('file-table-body');
    if (!tbody) return;

    // Sort rows
    const sortedRows = [...rows].sort((a, b) => {
      let aVal = a[sortField];
      let bVal = b[sortField];

      if (sortField === 'date') {
        aVal = new Date(aVal);
        bVal = new Date(bVal);
      }

      if (aVal < bVal) return sortOrder === 'asc' ? -1 : 1;
      if (aVal > bVal) return sortOrder === 'asc' ? 1 : -1;
      return 0;
    });

    tbody.innerHTML = sortedRows.map((row, index) => `
      <tr data-id="${row.id}" ${selectedRows.has(row.id) ? 'class="selected"' : ''}>
        <td>
          <input type="checkbox" class="checkbox row-checkbox" data-id="${row.id}"
            ${selectedRows.has(row.id) ? 'checked' : ''}>
        </td>
        <td class="editable" data-field="fileName" data-id="${row.id}">
           ${escapeHtml(row.fileName)}
        </td>
        <td>${formatFileSize(row.size)}</td>
        <td>${new Date(row.date).toLocaleDateString()}</td>
      </tr>
    `).join('');

    setupTableEventListeners();
  }

  function setupEventListeners() {
    // File upload
    const fileUpload = document.getElementById('file-upload');
    fileUpload.addEventListener('change', (e) => {
      handleFileUpload(e);
    });
  }

  function setupTableEventListeners() {
    // Row selection
    document.querySelectorAll('.row-checkbox').forEach(checkbox => {
      checkbox.addEventListener('change', (e) => {
        const id = parseInt(e.target.dataset.id);
        if (e.target.checked) {
          selectedRows.add(id);
        } else {
          selectedRows.delete(id);
        }
        updateRowSelection();
        updateToolbarState();
      });
    });

    // Cell editing
    document.querySelectorAll('.editable').forEach(cell => {
      cell.addEventListener('dblclick', (e) => {
        startCellEdit(e.target);
      });
    });
  }

  function setupFileListEventListeners() {
    // Select all
    const selectAll = document.getElementById('select-all');
    if (selectAll) {
      selectAll.addEventListener('change', (e) => {
        if (e.target.checked) {
          rows.forEach(row => selectedRows.add(row.id));
        } else {
          selectedRows.clear();
        }
        updateRowSelection();
        updateToolbarState();
        renderTableRows();
      });
    }

    // Column sorting
    document.querySelectorAll('.sortable').forEach(th => {
      th.addEventListener('click', (e) => {
        const field = e.target.dataset.field;
        if (sortField === field) {
          sortOrder = sortOrder === 'asc' ? 'desc' : 'asc';
        } else {
          sortField = field;
          sortOrder = 'asc';
        }
        updateSortIndicators();
        renderTableRows();
      });
    });

    // Toolbar buttons
    const uploadBtn = document.getElementById('upload-btn');
    const downloadBtn = document.getElementById('download-btn');
    const deleteBtn = document.getElementById('delete-btn');

    if (uploadBtn) {
      uploadBtn.addEventListener('click', () => {
        document.getElementById('file-upload').click();
      });
    }

    if (downloadBtn) {
      downloadBtn.addEventListener('click', () => {
        handleDownload();
      });
    }

    if (deleteBtn) {
      deleteBtn.addEventListener('click', () => {
        handleDelete();
      });
    }
  }

  function updateRowSelection() {
    document.querySelectorAll('tr[data-id]').forEach(row => {
      const id = parseInt(row.dataset.id);
      row.classList.toggle('selected', selectedRows.has(id));
    });

    document.querySelectorAll('.row-checkbox').forEach(checkbox => {
      const id = parseInt(checkbox.dataset.id);
      checkbox.checked = selectedRows.has(id);
    });

    const selectAll = document.getElementById('select-all');
    if (selectAll) {
      selectAll.checked = selectedRows.size === rows.length && rows.length > 0;
      selectAll.indeterminate = selectedRows.size > 0 && selectedRows.size < rows.length;
    }
  }

  function updateToolbarState() {
    const downloadBtn = document.getElementById('download-btn');
    const deleteBtn = document.getElementById('delete-btn');
    const selectionInfo = document.getElementById('selection-info');

    if (downloadBtn) {
      downloadBtn.disabled = selectedRows.size === 0;
    }

    if (deleteBtn) {
      deleteBtn.disabled = selectedRows.size !== 1;
    }

    if (selectionInfo) {
      const count = selectedRows.size;
      selectionInfo.textContent = count > 0 ? `${count} selected` : '';
    }
  }

  function updateSortIndicators() {
    document.querySelectorAll('.sort-indicator').forEach(indicator => {
      indicator.classList.remove('active');
      indicator.textContent = '↕';
    });

    const activeTh = document.querySelector(`th[data-field="${sortField}"] .sort-indicator`);
    if (activeTh) {
      activeTh.classList.add('active');
      activeTh.textContent = sortOrder === 'asc' ? '↑' : '↓';
    }
  }

  function startCellEdit(cell) {
    if (editingCell) return;

    editingCell = cell;
    const currentValue = cell.textContent.trim();
    const input = document.createElement('input');
    input.type = 'text';
    input.className = 'edit-input';
    input.value = currentValue;

    cell.innerHTML = '';
    cell.appendChild(input);
    input.focus();
    input.select();

    const finishEdit = (save = false) => {
      if (!editingCell) return;

      const newValue = input.value.trim();
      editingCell.textContent = save ? newValue : currentValue;

      if (save && newValue !== currentValue && newValue) {
        const id = parseInt(cell.dataset.id);
        const field = cell.dataset.field;
        handleRename(id, newValue);
      }

      editingCell = null;
    };

    input.addEventListener('blur', () => finishEdit(true));
    input.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        e.preventDefault();
        finishEdit(true);
      } else if (e.key === 'Escape') {
        e.preventDefault();
        finishEdit(false);
      }
    });
  }

  async function handleLogin() {
    const tokenInput = document.getElementById('token-input');
    const submitBtn = document.getElementById('submit-btn');
    const helperText = document.getElementById('helper-text');
    const tokenText = tokenInput.value.trim();
    if (!tokenText) return;

    submitBtn.disabled = true;
    submitBtn.innerHTML = '<span class="loading"></span> Logging in...';

    try {
      const data = await callApi('/api/login', `token=${tokenText}`);
      token = tokenText;
      rows = data;
      renderContent();
    } catch (error) {
      tokenInput.classList.add('error');
      helperText.textContent = 'Invalid token. Enter the access token displayed on the SmallBASIC [About] screen.';
      helperText.classList.add('error');
      showSnackbar(error, 'error');
    } finally {
      submitBtn.disabled = false;
      submitBtn.textContent = 'Submit';
    }
  }

  async function handleFileUpload(event) {
    const files = Array.from(event.target.files);
    if (files.length === 0) return;

    showSnackbar('Uploading files...', 'info');

    try {
      for (let i = 0; i < files.length; i++) {
        const file = files[i];
        const dataUrl = await fileToDataURL(file);
        await callApi('/api/upload',
                           `fileName=${encodeURIComponent(file.name)}&data=${dataUrl}`);

        if (files.length > 1) {
          showSnackbar(`Uploaded ${i + 1}/${files.length} files`, 'info');
        }
      }

      const data = await callApi('/api/files', '');
      rows = data;
      selectedRows.clear();
      renderTableRows();
      updateToolbarState();
      showSnackbar(`Successfully uploaded ${files.length} file(s)`, 'success');
    } catch (error) {
      showSnackbar(error, 'error');
      // Refresh to show any partial uploads
      try {
        const data = await callApi('/api/files', '');
        rows = data;
        renderTableRows();
      } catch (e) {}
    }

    // Reset file input
    event.target.value = '';
  }

  function handleDownload() {
    if (selectedRows.size === 0) return;

    let filename = 'smallbasic-files.zip';
    let params = '';

    if (selectedRows.size === 1) {
      const selectedRow = rows.find(row => selectedRows.has(row.id));
      if (selectedRow) {
        filename = selectedRow.fileName;
      }
    }

    if (selectedRows.size === rows.length) {
      params = 'all=t';
    } else {
      const fileParams = [];
      selectedRows.forEach(id => {
        const row = rows.find(r => r.id === id);
        if (row) {
          fileParams.push(`f=${encodeURIComponent(row.fileName)}`);
        }
      });
      params = fileParams.join('&');
    }

    const link = document.createElement('a');
    link.href = `./api/download?${params}`;
    link.download = filename;
    link.click();
  }

  async function handleDelete() {
    if (selectedRows.size !== 1) return;

    const selectedRow = rows.find(row => selectedRows.has(row.id));
    if (!selectedRow) return;

    if (await showConfirmDialog(
      'Delete file',
      `Are you sure you want to permanently delete ${selectedRow.fileName}? You cannot undo `
    )) {
      try {
        const data = await callApi('/api/delete',
                                        `fileName=${encodeURIComponent(selectedRow.fileName)}`);
        rows = data;
        selectedRows.clear();
        renderTableRows();
        updateToolbarState();
        showSnackbar('File deleted successfully', 'success');
      } catch (error) {
        showSnackbar(error, 'error');
      }
    }
  }

  async function handleRename(id, newName) {
    const row = rows.find(r => r.id === id);
    if (!row || !newName.trim()) return;

    const oldName = row.fileName;
    if (oldName === newName) return;

    try {
      await callApi('/api/rename',
                         `from=${encodeURIComponent(oldName)}&to=${encodeURIComponent(newName)}`);
      row.fileName = newName;
      showSnackbar('File renamed successfully', 'success');
    } catch (error) {
      showSnackbar(error, 'error');
      // Refresh to revert the change
      renderTableRows();
    }
  }

  async function callApi(endpoint, body) {
    const response = await fetch(endpoint, {
      method: 'POST',
      headers: {'Content-Type': 'application/text;charset=utf-8'},
      body: body
    });

    const data = await response.json();
    if (data.error) {
      throw new Error(data.error);
    }
    return data;
  }

  function fileToDataURL(file) {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = () => resolve(reader.result);
      reader.onerror = reject;
      reader.readAsDataURL(file);
    });
  }

  function showSnackbar(message, type = 'info') {
    // Remove existing snackbar
    const existing = document.querySelector('.snackbar');
    if (existing) {
      existing.remove();
    }

    const snackbar = document.createElement('div');
    snackbar.className = `snackbar ${type}`;
    snackbar.textContent = message;
    document.body.appendChild(snackbar);

    setTimeout(() => {
      if (snackbar.parentNode) {
        snackbar.remove();
      }
    }, 5000);
  }

  function showConfirmDialog(title, message) {
    return new Promise(resolve => {
      const overlay = document.createElement('div');
      overlay.className = 'dialog-overlay';
      overlay.innerHTML = `
        <div class="dialog">
          <h2>${escapeHtml(title)}</h2>
          <p>${escapeHtml(message)}</p>
          <div class="dialog-actions">
            <button class="btn" id="dialog-cancel">Cancel</button>
            <button class="btn" id="dialog-confirm">Delete</button>
          </div>
        </div>
      `;

      document.body.appendChild(overlay);

      const handleClose = (confirmed) => {
        overlay.remove();
        resolve(confirmed);
      };

      document.getElementById('dialog-cancel').addEventListener('click', () => handleClose(false));
      document.getElementById('dialog-confirm').addEventListener('click', () => handleClose(true));
      overlay.addEventListener('click', (e) => {
        if (e.target === overlay) handleClose(false);
      });
    });
  }

  function formatFileSize(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
  }

  function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }
}());
