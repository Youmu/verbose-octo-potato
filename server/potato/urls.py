from django.urls import path
from . import views

urlpatterns = [
    path('potato/msg', views.msg_handler, name='msg_handler'),
    path('Potato/view.html', views.view_page, name='view_page'),
]
