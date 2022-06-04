<?php

	header("Status: 200 OK");

	$title = $_GET['title'];
	$content = $_GET['content'];

	echo "title: $title<br>";
	echo "content: $content";

?>