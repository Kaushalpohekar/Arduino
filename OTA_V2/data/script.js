const sidebarToggle = document.querySelector('.sidebar-toggle');
const sidebar = document.querySelector('.sidebar');
const mainContent = document.querySelector('.main-content');

sidebarToggle.addEventListener('click', () => {
    console.log('Button clicked'); // Add this line for debugging

    sidebar.classList.toggle('sidebar-open');
    mainContent.classList.toggle('main-content-hidden');

    // Toggle the content of the toggle button
    if (sidebar.classList.contains('sidebar-open')) {
        sidebarToggle.innerHTML = '&#10005;'; // Display close icon
    } else {
        sidebarToggle.innerHTML = '&#9776;'; // Display menu icon
    }
});

